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
    createDepthTarget();
    createFrames();
    NXT_LOG_INFO(Engine, FrameLoop, "frame loop created: lanes={}", imageCount_);
}

FrameLoop::~FrameLoop() {
    // fence만 기다리면 부족하다. fence는 submit 완료를 뜻하고, present는 큐에 남아
    // 있을 수 있다. 그 상태로 present semaphore를 파괴하면 검증 레이어가 막는다
    // (VUID-vkDestroySemaphore-semaphore-05149). swapchain도 이 뒤에 파괴되므로
    // 같은 대기가 필요하다 (VUID-vkDestroySwapchainKHR-swapchain-01282).
    waitAllFences();
    device_.waitIdle();
    deletion_->flush();
    destroyFrames();
    destroyDepthTarget();
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

    // 3. record. encoder는 레인마다 하나씩 미리 만들어 두고 재사용한다. 기록이 끝났다고
    //    encoder를 버리면 안 된다 — 그 소멸자가 recording 중인 command buffer가 참조하는
    //    render pass를 파괴한다 (VUID-vkEndCommandBuffer-commandBuffer-00059).
    rhi::beginCommandBuffer(device_, commandBuffer);
    record_(*encoders_[frameIndex], FrameTarget{.color = swapchain_.imageView(imageIndex),
                                                .colorFormat = swapchain_.format(),
                                                .depth = depthView,
                                                .depthFormat = kDepthFormat,
                                                .extent = swapchain_.extent(),
                                                .frameIndex = frameIndex});
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
    // frame.md §6: in-flight fence를 모두 기다린다. 여기에 더해 device를 idle로 만든다 —
    // fence는 present 완료를 보장하지 않는데, present가 큐에 남은 채로 swapchain을
    // 파괴하면 VUID-vkDestroySwapchainKHR-swapchain-01282가 난다. 리사이즈는 드물어서
    // 전체 대기 비용이 문제되지 않는다.
    waitAllFences();
    device_.waitIdle();
    deletion_->flush();

    // 레인을 swapchain보다 먼저 버린다. encoder가 든 framebuffer가 swapchain의 image
    // view를 참조하므로, view를 파괴하는 swapchain 재생성보다 앞서야 한다.
    destroyFrames();

    // 깊이 이미지도 이전 크기에 묶여 있다. 새 extent로 다시 만들기 전에 버린다.
    destroyDepthTarget();

    swapchain_.recreate();

    createDepthTarget();

    // 이미지 개수가 같아도 다시 만든다. present가 실패하면 그 semaphore의 wait이
    // 수행되지 않아 신호된 채 남을 수 있고, 남은 신호를 물려받으면 다음 제출이
    // 거부된다 (VUID-vkQueueSubmit-pSignalSemaphores-00067). 새로 만들면 그 상태가
    // 시작부터 없다.
    createFrames();
    NXT_LOG_INFO(Engine, FrameLoop, "frame loop recreated: lanes={}", imageCount_);
}

rhi::ResourceDeletion& FrameLoop::deletion() noexcept {
    return *deletion_;
}

void FrameLoop::createDepthTarget() {
    const rhi::Extent2D extent = swapchain_.extent();
    NXT_ASSERT_MSG((extent.width > 0) && (extent.height > 0), "크기가 0인 깊이 이미지를 만들려 했다.");

    depthImage_ = rhi::createImage(device_, rhi::ImageDesc{
                                                .extent = extent,
                                                .format = kDepthFormat,
                                                .usage = rhi::ImageUsage::DepthStencilAttachment,
                                            });

    depthView = rhi::createImageView(device_, rhi::ImageViewDesc{
                                                  .image = depthImage_.image,
                                                  .format = kDepthFormat,
                                              });

    NXT_LOG_DEBUG(Engine, FrameLoop, "depth target created: {}x{}", extent.width, extent.height);
}

void FrameLoop::destroyDepthTarget() {
    // view를 이미지보다 먼저 지운다 (image.hpp). framebuffer가 view를 참조하므로
    // destroyFrames()가 먼저 불려야 한다.
    if (depthView.valid()) {
        rhi::destroyImageView(device_, depthView);
        depthView = rhi::ImageView{};
    }
    if (depthImage_.image.valid()) {
        rhi::destroyImage(device_, depthImage_);
        depthImage_ = rhi::ImageAllocation{};
    }
}

void FrameLoop::createFrames() {
    imageCount_ = swapchain_.imageCount();
    NXT_ASSERT_MSG(imageCount_ > 0, "이미지가 없는 swapchain으로 프레임 레인을 만들려 했다.");

    commandBuffers_.reserve(imageCount_);
    encoders_.reserve(imageCount_);
    fences_.reserve(imageCount_);
    acquireSemaphores_.reserve(imageCount_);
    presentSemaphores_.reserve(imageCount_);

    for (std::uint32_t index = 0; index < imageCount_; ++index) {
        const rhi::CommandBuffer commandBuffer = rhi::createCommandBuffer(device_);
        commandBuffers_.push_back(commandBuffer);
        // encoder는 command buffer보다 오래 살아야 한다. 레인에 묶어 두고 재사용한다.
        encoders_.push_back(rhi::CommandEncoder::create(device_, commandBuffer));

        fences_.push_back(rhi::createFence(device_));
        acquireSemaphores_.push_back(rhi::createSemaphore(device_));
        presentSemaphores_.push_back(rhi::createSemaphore(device_));
    }

    frameIndex_ = 0;
    NXT_LOG_DEBUG(Engine, FrameLoop, "frame lanes created: count={}", imageCount_);
}

void FrameLoop::destroyFrames() {
    // encoder를 가장 먼저 버린다. render pass와 framebuffer를 소유하고, framebuffer는
    // swapchain이 소유한 image view를 참조한다. 호출자는 이미 waitAllFences()와
    // waitIdle()을 마쳤으므로 실행 중인 명령은 없다.
    encoders_.clear();

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
