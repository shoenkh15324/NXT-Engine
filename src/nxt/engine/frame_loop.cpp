#include <cstdint>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/engine/frame_loop.hpp>
#include <optional>
#include <utility>

namespace nxt::engine {

std::unique_ptr<FrameLoop> FrameLoop::create(const rhi::Device& device, rhi::Swapchain& swapchain,
                                             RecordFunction record) {
    NXT_ASSERT_MSG(record != nullptr, "그릴 내용을 기록할 함수가 없다.");
    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다 (rhi factory와 같은 이유)
    return std::unique_ptr<FrameLoop>(new FrameLoop(device, swapchain, std::move(record)));
}

FrameLoop::FrameLoop(const rhi::Device& device, rhi::Swapchain& swapchain, RecordFunction record)
    : device_(device),
      swapchain_(swapchain),
      record_{std::move(record)},
      deletion_{rhi::ResourceDeletion::create(device)} {
    createFrames();
    NXT_LOG_INFO(Engine, FrameLoop, "frame loop created: lanes={}", imageCount_);
}

FrameLoop::~FrameLoop() {
    // 마지막 제출이 아직 GPU에 있을 수 있다. 여기서 기다리지 않으면 아래 두 줄이
    // in-flight 리소스를 파괴한다 (frame.md §9).
    waitAllFences();
    deletion_->flush();
    destroyFrames();
}

FrameLoop::Status FrameLoop::frame() {
    const std::uint32_t frameIndex = frameIndex_;
    const rhi::Fence& fence = fences_[frameIndex];
    const rhi::CommandBuffer& commandBuffer = commandBuffers_[frameIndex];

    // 1. 백프레시. 이 레인의 이전 제출이 끝나야 "그 프레임이 쓴 리소스는 이제 안전하다"를
    //    말할 수 있다 (frame.md §5, §9).
    rhi::waitFence(device_, fence);
    deletion_->collect(frameIndex);

    // 2. acquire. 실패하면 아직 아무것도 제출하지 않았으므로 fence를 건드리지 않고 돌아간다.
    //    여기서 reset하면 신호되지 않은 fence가 남아 다음 waitFence가 영원히 멈춘다.
    const std::optional<std::uint32_t> acquired = swapchain_.acquireNextImage(acquireSemaphores_[frameIndex]);
    if (!acquired) {
        NXT_LOG_DEBUG(Engine, FrameLoop, "frame skipped: swapchain out of date");
        return Status::OutOfDate;
    }
    const std::uint32_t imageIndex = *acquired;

    // 3. record. encoder는 render pass와 framebuffer를 소유하므로 endCommandBuffer 전에
    //    반드시 파괴한다 (command_buffer.hpp).
    rhi::beginCommandBuffer(device_, commandBuffer);
    {
        const std::unique_ptr<rhi::CommandEncoder> encoder = rhi::CommandEncoder::create(device_, commandBuffer);
        record_(*encoder, FrameTarget{.color = swapchain_.imageView(imageIndex),
                                      .colorFormat = swapchain_.format(),
                                      .extent = swapchain_.extent(),
                                      .frameIndex = frameIndex});
    }
    rhi::endCommandBuffer(device_, commandBuffer);

    // 4. submit. fence는 제출 직전에 되돌린다.
    rhi::resetFence(device_, fence);
    rhi::submit(device_, commandBuffer,
                rhi::SubmitDesc{.waitSemaphore = acquireSemaphores_[frameIndex],
                                .signalSemaphore = presentSemaphores_[imageIndex],
                                .fence = fence});

    // 5. present. 실패해도 제출은 이미 큐에 있으므로 레인은 넘긴다.
    //    실패했다면 presentSemaphores_[imageIndex]의 wait이 수행되지 않았을 수 있어
    //    그 semaphore가 신호된 채 남는다. recreate()가 이 상태를 어떻게 다루는지는
    //    그쪽 주석에 적혀 있다.
    const bool presented = swapchain_.present(imageIndex, presentSemaphores_[imageIndex]);
    frameIndex_ = (frameIndex_ + 1) % imageCount_;

    if (!presented) {
        NXT_LOG_DEBUG(Engine, FrameLoop, "frame presented out of date: image={}", imageIndex);
        return Status::OutOfDate;
    }
    return Status::Presented;
}

void FrameLoop::recreate() {
    // frame.md §6: in-flight fence를 모두 기다린 뒤에 파괴하고 다시 만든다.
    waitAllFences();
    deletion_->flush();
    swapchain_.recreate();

    // 최소화 상태에서는 recreate()가 아무것도 바꾸지 않는다. 그때는 레인도 그대로 둔다.
    //
    // 이미지 개수가 같으면 present semaphore를 다시 만들지 않는다. present가 실패하면
    // 그 wait이 수행되지 않아 semaphore가 신호된 채 남을 수 있으므로, A7에서 리사이즈를
    // 반복하며 검증 레이어가 이 상태를 문제 삼지 않는지 확인한다.
    if (swapchain_.imageCount() == imageCount_) {
        NXT_LOG_DEBUG(Engine, FrameLoop, "frame lanes unchanged: count={}", imageCount_);
        return;
    }

    destroyFrames();
    createFrames();
    NXT_LOG_INFO(Engine, FrameLoop, "frame loop recreated: lanes={}", imageCount_);
}

rhi::ResourceDeletion& FrameLoop::deletion() noexcept {
    return *deletion_;
}

void FrameLoop::createFrames() {
    imageCount_ = swapchain_.imageCount();
    NXT_ASSERT_MSG(imageCount_ > 0, "이미지가 없는 swapchain으로 프레임 레인을 만들려 했다.");

    commandBuffers_.reserve(imageCount_);
    fences_.reserve(imageCount_);
    acquireSemaphores_.reserve(imageCount_);
    presentSemaphores_.reserve(imageCount_);

    for (std::uint32_t index = 0; index < imageCount_; ++index) {
        commandBuffers_.push_back(rhi::createCommandBuffer(device_));
        fences_.push_back(rhi::createFence(device_));
        acquireSemaphores_.push_back(rhi::createSemaphore(device_));
        presentSemaphores_.push_back(rhi::createSemaphore(device_));
    }

    frameIndex_ = 0;
    NXT_LOG_DEBUG(Engine, FrameLoop, "frame lanes created: count={}", imageCount_);
}

void FrameLoop::destroyFrames() {
    for (const rhi::Semaphore& semaphore : presentSemaphores_) {
        rhi::destroySemaphore(device_, semaphore);
    }
    for (const rhi::Semaphore& semaphore : acquireSemaphores_) {
        rhi::destroySemaphore(device_, semaphore);
    }
    for (const rhi::Fence& fence : fences_) {
        rhi::destroyFence(device_, fence);
    }
    for (const rhi::CommandBuffer& commandBuffer : commandBuffers_) {
        rhi::destroyCommandBuffer(device_, commandBuffer);
    }

    presentSemaphores_.clear();
    acquireSemaphores_.clear();
    fences_.clear();
    commandBuffers_.clear();
}

void FrameLoop::waitAllFences() {
    for (const rhi::Fence& fence : fences_) {
        rhi::waitFence(device_, fence);
    }
}

} // namespace nxt::engine
