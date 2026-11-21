#include <algorithm>
#include <cstdint>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/engine/frame_loop.hpp>
#include <optional>
#include <span>
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
    // 1-A2는 CPU 구간과 GPU 시간을 같이 보려고 계측을 켠 채로 돈다. 런타임 토글과
    // ON/OFF 오버헤드 측정은 1-C5에서 붙인다.
    profiler_.setEnabled(true);
    core::diag::Profiler::setCurrent(&profiler_);

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

    // 이 스레드의 현재 Profiler를 비운다. 이 객체가 사라진 뒤에 Scope가 남아 있으면
    // 매달린 포인터를 쓰게 된다.
    core::diag::Profiler::setCurrent(nullptr);
}

FrameLoop::Status FrameLoop::frame() {
    const std::uint32_t frameIndex = frameIndex_;
    const rhi::Fence& fence = fences_[frameIndex];
    const rhi::CommandBuffer& commandBuffer = commandBuffers_[frameIndex];
    const rhi::TimestampQueryPool& timestamps = timestampPools_[frameIndex];
    rhi::CommandEncoder& encoder = *encoders_[frameIndex];

    // cpu frame은 프레임 시작에서 submit 반환이 끝나는 지점까지다 (phase1.md §6.1).
    const core::time::Timestamp frameStart = core::time::now();

    // 1. 백프레시. 이 레인의 이전 제출이 끝나야 "그 프레임이 쓴 리소스는 이제 안전하다"를
    //    말할 수 있다 (frame.md §5, §9).
    const core::time::Timestamp fenceStart = core::time::now();
    rhi::waitFence(device_, fence);
    const std::uint64_t fenceWaitNs = (core::time::now() - fenceStart).ns();
    deletion_->collect(frameIndex);

    // fence가 신호됐으므로 이 레인이 지난번에 찍은 timestamp를 stall 없이 읽을 수 있다
    // (phase1.md §5.1). 그 표본의 GPU 시간을 여기서 채운다.
    completePendingSample(frameIndex, timestamps);

    // 2. acquire. 실패하면 아직 아무것도 제출하지 않았으므로 fence를 건드리지 않고 돌아간다.
    //    여기서 reset하면 신호되지 않은 fence가 남아 다음 waitFence가 영원히 멈춘다.
    const std::optional<std::uint32_t> acquired = swapchain_.acquireNextImage(acquireSemaphores_[frameIndex]);
    if (!acquired) {
        NXT_LOG_DEBUG(Engine, FrameLoop, "frame skipped: swapchain out of date");
        return Status::OutOfDate;
    }
    const std::uint32_t imageIndex = *acquired;

    // 3. record. timestamp는 render pass 밖에서 찍는다.
    rhi::beginCommandBuffer(device_, commandBuffer);
    if (timestamps.pool.valid()) {
        encoder.resetQueryPool(timestamps.pool, 0, kTimestampsPerFrame);
        encoder.writeTimestamp(timestamps.pool, 0);
    }

    profiler_.beginFrame();
    std::uint32_t drawCalls = 0;
    {
        NXT_PROFILE_SCOPE("record");
        drawCalls = record_(*encoders_[frameIndex], FrameTarget{.color = swapchain_.imageView(imageIndex),
                                                                .colorFormat = swapchain_.format(),
                                                                .depth = depthView,
                                                                .depthFormat = kDepthFormat,
                                                                .extent = swapchain_.extent(),
                                                                .frameIndex = frameIndex});
    }

    if (timestamps.pool.valid()) {
        encoder.writeTimestamp(timestamps.pool, 1);
    }
    rhi::endCommandBuffer(device_, commandBuffer);

    // 4. submit. fence는 제출 직전에 되돌린다.
    rhi::resetFence(device_, fence);
    {
        NXT_PROFILE_SCOPE("submit");
        rhi::submit(device_, commandBuffer,
                    rhi::SubmitDesc{.waitSemaphore = acquireSemaphores_[frameIndex],
                                    .signalSemaphore = presentSemaphores_[imageIndex],
                                    .fence = fence});
    }
    profiler_.endFrame();
    const core::time::Timestamp submitted = core::time::now();

    // 5. present. 실패해도 제출은 이미 큐에 있으므로 레인은 넘긴다.
    const bool presented = swapchain_.present(imageIndex, presentSemaphores_[imageIndex]);

    storePendingSample(frameIndex, frameStart, submitted, fenceWaitNs, drawCalls);
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
    timestampPools_.reserve(imageCount_);
    pendingSamples_.reserve(imageCount_);

    for (std::uint32_t index = 0; index < imageCount_; ++index) {
        const rhi::CommandBuffer commandBuffer = rhi::createCommandBuffer(device_);
        commandBuffers_.push_back(commandBuffer);
        encoders_.push_back(rhi::CommandEncoder::create(device_, commandBuffer));

        fences_.push_back(rhi::createFence(device_));
        acquireSemaphores_.push_back(rhi::createSemaphore(device_));
        presentSemaphores_.push_back(rhi::createSemaphore(device_));

        // 레인마다 start/end 한 쌍이면 충분하다. draw마다 찍으면 측정 대상이 바뀐다 (phase1.md §5.1).
        timestampPools_.push_back(rhi::createTimestampQueryPool(device_, kTimestampsPerFrame));
        pendingSamples_.push_back(PendingFrameSample{});
    }

    timestampsSupported_ = !timestampPools_.empty() && timestampPools_.front().pool.valid();
    frameIndex_ = 0;

    NXT_LOG_DEBUG(Engine, FrameLoop, "frame lanes created: count={}", imageCount_);
    if (timestampsSupported_) {
        NXT_LOG_INFO(Engine, FrameLoop, "gpu timestamps: supported (period={}ns)", timestampPools_.front().periodNs);
    } else {
        NXT_LOG_INFO(Engine, FrameLoop, "gpu timestamps: unsupported, gpu frame falls back to fence wait");
    }
}

void FrameLoop::destroyFrames() {
    // encoder를 가장 먼저 버린다. render pass와 framebuffer를 소유하고, framebuffer는
    // swapchain이 소유한 image view를 참조한다. 호출자는 이미 waitAllFences()와
    // waitIdle()을 마쳤으므로 실행 중인 명령은 없다.
    encoders_.clear();

    for (const rhi::TimestampQueryPool& pool : timestampPools_) {
        rhi::destroyTimestampQueryPool(device_, pool);
    }
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
    timestampPools_.clear();
    pendingSamples_.clear();
}

void FrameLoop::waitAllFences() {
    for (const rhi::Fence& fence : fences_) {
        rhi::waitFence(device_, fence);
    }
}

FrameStats& FrameLoop::stats() noexcept {
    return stats_;
}

void FrameLoop::completePendingSample(std::uint32_t frameIndex, const rhi::TimestampQueryPool& timestamps) {
    PendingFrameSample& pending = pendingSamples_[frameIndex];
    if (!pending.valid) {
        return;
    }

    if (timestamps.pool.valid()) {
        const std::optional<std::uint64_t> gpuNs = rhi::readElapsedNs(device_, timestamps, 0, 1);
        pending.sample.gpuFrameNs = gpuNs.value_or(0);
        pending.sample.gpuApproximate = false;
    } else {
        // timestamp를 못 쓰는 GPU에서는 fence wait를 GPU 시간의 근사치로 쓴다.
        // (phase1.md §5.1). 로그는 이 값을 "~"로 표시한다.
        pending.sample.gpuFrameNs = pending.sample.fenceWaitNs;
        pending.sample.gpuApproximate = true;
    }

    stats_.add(pending.sample);
    pending.valid = false;
}

void FrameLoop::storePendingSample(std::uint32_t frameIndex, core::time::Timestamp frameStart,
                                   core::time::Timestamp submitted, std::uint64_t fenceWaitNs,
                                   std::uint32_t drawCalls) noexcept {
    PendingFrameSample& pending = pendingSamples_[frameIndex];

    pending.sample.cpuFrameNs = (submitted - frameStart).ns();
    pending.sample.gpuFrameNs = 0;
    pending.sample.fenceWaitNs = fenceWaitNs;
    pending.sample.drawCalls = drawCalls;
    pending.sample.gpuApproximate = false;

    const std::span<const core::diag::Sample> segments = profiler_.samples();
    pending.sample.segmentCount = segments.size();
    std::copy(segments.begin(), segments.end(), pending.sample.segments.begin());

    pending.valid = true;
}

} // namespace nxt::engine
