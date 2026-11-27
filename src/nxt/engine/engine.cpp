#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/core/diagnostics/profiler.hpp>
#include <nxt/core/time/timer.hpp>
#include <nxt/engine/engine.hpp>
#include <nxt/math/frustum.hpp>
#include <string>
#include <thread>
#include <utility>

namespace nxt::engine {

namespace {

/// @brief 프레임 요약을 로그로 남기는 주기(밀리초)이다.
constexpr std::uint64_t kReportIntervalMs = 1000;

} // namespace

std::unique_ptr<Engine> Engine::create(const EngineDesc& desc) {
    auto window = platform::window::createWindow(desc.window);
    if (!window) {
        NXT_LOG_ERROR(Engine, None, "window creation failed: {}", desc.window.title);
        return nullptr;
    }

    NXT_LOG_INFO(Platform, Window, "window ready : {}x{} \"{}\"", window->width(), window->height(), desc.window.title);

    return std::unique_ptr<Engine>{new Engine(desc, std::move(window))};
}

Engine::Engine(const EngineDesc& desc, std::unique_ptr<platform::window::Window> window) : window_(std::move(window)) {
    // surface는 창의 핸들을 참조하므로 창을 만든 뒤에 만든다. device와 swapchain은
    // surface를 거쳐 창에 묶인다.
    instance_ = rhi::Instance::create(rhi::InstanceDesc{.appName = desc.appName});
    surface_ = instance_->createSurface(window_->native());
    device_ = instance_->createDevice(rhi::DeviceDesc{.surface = surface_});
    swapchain_ = rhi::Swapchain::create(
        *device_, rhi::SwapchainDesc{.surface = surface_,
                                     .presentMode = desc.presentMode,
                                     .extent = rhi::Extent2D{window_->width(), window_->height()}});

    // 파이프라인이 두 부착물의 형식을 알아야 하므로 swapchain 뒤에 만든다. 깊이 형식은
    // 프레임 루프가 이미지를 만들 때 쓰는 값과 같아야 한다.
    renderer_ = std::make_unique<renderer::Renderer>(*device_, swapchain_->format(), FrameLoop::kDepthFormat,
                                                     desc.vertexShader, desc.fragmentShader);

    // 프레임 루프는 렌더러와 월드를 참조로 잡는다. 둘 다 프레임 루프보다 오래
    // 살아야 하므로 멤버 선언 순서도 그에 맞춘다.
    renderer::Renderer* sceneRenderer = renderer_.get();
    renderer::World* sceneWorld = &world_;
    renderer::DrawList* sceneDrawList = &drawList_;
    const bool culling = desc.culling;

    frameLoop_ = FrameLoop::create(
        *device_, *swapchain_,
        [sceneRenderer, sceneWorld, sceneDrawList, culling](rhi::CommandEncoder& encoder, const FrameTarget& target) {
            // 컬링 비용을 기록 비용과 분리해 잰다. record 구간은 컬링을 포함한 합계이므로,
            // 1-B8(병렬 컬링)의 이득을 재려면 이 구간이 따로 있어야 한다 (phase1.md §3).
            {
                NXT_PROFILE_SCOPE("cull");
                renderer::buildDrawList(*sceneWorld, math::Frustum::clipSpace(), culling, *sceneDrawList);
            }
            return sceneRenderer->record(encoder,
                                         renderer::FrameContext{
                                             .color = target.color,
                                             .colorFormat = target.colorFormat,
                                             .depth = target.depth,
                                             .depthFormat = target.depthFormat,
                                             .extent = target.extent,
                                             .drawList = sceneDrawList->objects(),
                                         },
                                         *sceneWorld);
        });
}

Engine::~Engine() {
    // 소유 관계의 역순이다. 프레임 루프가 fence를 기다리고 waitIdle까지 마치므로 그
    // 뒤에 렌더러의 버퍼를 버린다. swapchain이 이미지와 view를 소유하고, surface는
    // instance가 파괴한다 (frame.md §9).
    frameLoop_.reset();
    renderer_.reset();
    swapchain_.reset();
    device_.reset();
    instance_->destroySurface(surface_);
    instance_.reset();
    window_.reset();
}

void Engine::run() {
    core::time::Timer reportTimer;

    while (pumpFrame()) {
        if (reportTimer.elapsedMs() < kReportIntervalMs) {
            continue;
        }
        logFrameSummary();
        reportTimer.reset();
    }
    // 1초보다 짧게 끝나도 숫자를 남긴다. 닫자마다 결과를 볼 수 있어야 한다.
    logFrameSummary();
    NXT_LOG_INFO(Platform, Window, "window closed");
}

void Engine::logFrameSummary() {
    const FrameSummary summary = frameLoop_->stats().summary();
    frameLoop_->stats().clear();

    if (summary.frames == 0) {
        // 워밍업 중에는 버린 표본만 있어 요약할 것이 없다. 왜 로그가 없는지 알 수 있게
        // 남은 프레임 수를 알린다. clear()는 워밍업 카운터를 건드리지 않는다.
        if (frameLoop_->stats().isWarmingUp()) {
            NXT_LOG_INFO(Engine, FrameLoop, "warmup: {} frames left", frameLoop_->stats().warmupRemaining());
        }
        return;
    }

    // 평균 대신 중앙값과 p95를 남긴다. 평균은 한두 번 튀는 프레임에 끌려가서 "이 설정의
    // 비용"을 대표하지 못한다 (phase1.md §3).
    NXT_LOG_INFO(Engine, FrameLoop,
                 "frame: n={} cpu p50={:.2f} p95={:.2f}ms gpu{} p50={:.2f} p95={:.2f}ms fence p50={:.2f} "
                 "p95={:.2f}ms draws={}",
                 summary.frames, summary.cpuFrameMs, summary.cpuFrameP95Ms, summary.gpuApproximate ? "~" : "",
                 summary.gpuFrameMs, summary.gpuFrameP95Ms, summary.fenceWaitMs, summary.fenceWaitP95Ms,
                 summary.drawCalls);

    if (summary.segmentCount == 0) {
        return;
    }

    std::string segments;
    for (std::size_t index = 0; index < summary.segmentCount; ++index) {
        if (index != 0) {
            segments += ' ';
        }
        segments += std::format("{} p50={:.2f} p95={:.2f}ms", summary.segments[index].name,
                                summary.segments[index].medianMs, summary.segments[index].p95Ms);
    }
    NXT_LOG_INFO(Engine, FrameLoop, "cpu : {}", segments);
}

renderer::World& Engine::world() noexcept {
    return world_;
}

bool Engine::pumpFrame() {
    window_->pollEvents();
    if (!window_->isOpen()) {
        return false;
    }

    // 최소화. 그릴 영역이 없어 acquire가 계속 실패하므로 프레임을 부르지 않는다.
    // pollEvents는 이벤트를 기다리지 않고 즉시 돌아오므로, 여기서 쉬지 않으면 CPU를 100% 쓴다.
    if ((window_->width() == 0) || (window_->height() == 0)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return true;
    }

    // 창 크기와 swapchain 크기가 다르면 낡은 것이다. fence 대기는 recreate가 한다.
    const rhi::Extent2D windowExtent{window_->width(), window_->height()};
    const rhi::Extent2D extent = swapchain_->extent();
    if ((windowExtent.width != extent.width) || (windowExtent.height != extent.height)) {
        frameLoop_->recreate(windowExtent);
    }

    // acquire/present가 out-of-date를 돌려주는 경우. 루프는 스스로 고치지 않고
    // 여기서 recreate를 부른다 (frame.md §6).
    if (frameLoop_->frame() == FrameLoop::Status::OutOfDate) {
        NXT_LOG_INFO(Engine, FrameLoop, "swapchain out of date: recreating");
        frameLoop_->recreate(windowExtent);
    }

    return true;
}

} // namespace nxt::engine
