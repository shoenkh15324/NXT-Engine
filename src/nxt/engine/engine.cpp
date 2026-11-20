#include <chrono>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/core/time/timer.hpp>
#include <nxt/engine/engine.hpp>
#include <thread>
#include <utility>

namespace nxt::engine {

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
    swapchain_ = rhi::Swapchain::create(*device_, rhi::SwapchainDesc{.surface = surface_});

    // 파이프라인이 두 부착물의 형식을 알아야 하므로 swapchain 뒤에 만든다. 깊이 형식은
    // 프레임 루프가 이미지를 만들 때 쓰는 값과 같아야 한다.
    renderer_ = std::make_unique<renderer::Renderer>(*device_, swapchain_->format(), FrameLoop::kDepthFormat,
                                                     desc.vertexShader, desc.fragmentShader);

    // 프레임 루프는 렌더러와 월드를 참조로 잡는다. 둘 다 프레임 루프보다 오래
    // 살아야 하므로 멤버 선언 순서도 그에 맞춘다.
    renderer::Renderer* sceneRenderer = renderer_.get();
    renderer::World* sceneWorld = &world_;
    frameLoop_ = FrameLoop::create(
        *device_, *swapchain_, [sceneRenderer, sceneWorld](rhi::CommandEncoder& encoder, const FrameTarget& target) {
            sceneRenderer->record(encoder,
                                  renderer::FrameContext{.color = target.color,
                                                         .colorFormat = target.colorFormat,
                                                         .depth = target.depth,
                                                         .depthFormat = target.depthFormat,
                                                         .extent = target.extent},
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
    core::time::Timer fpsTimer;
    std::uint32_t frames = 0;

    while (pumpFrame()) {
        ++frames;
        if (fpsTimer.elapsedMs() >= 1000) {
            NXT_LOG_INFO(Engine, FrameLoop, "fps={}", frames);
            frames = 0;
            fpsTimer.reset();
        }
    }

    NXT_LOG_INFO(Platform, Window, "window closed");
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
    // pollEvents는 PeekMessage라 여기서 쉬지 않으면 CPU를 100% 쓴다.
    if ((window_->width() == 0) || (window_->height() == 0)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return true;
    }

    // 창 크기와 swapchain 크기가 다르면 낡은 것이다. fence 대기는 recreate가 한다.
    const rhi::Extent2D extent = swapchain_->extent();
    if ((window_->width() != extent.width) || (window_->height() != extent.height)) {
        frameLoop_->recreate();
    }

    // acquire/present가 out-of-date를 돌려주는 경우. 루프는 스스로 고치지 않고
    // 여기서 recreate를 부른다 (frame.md §6).
    if (frameLoop_->frame() == FrameLoop::Status::OutOfDate) {
        NXT_LOG_INFO(Engine, FrameLoop, "swapchain out of date: recreating");
        frameLoop_->recreate();
    }

    return true;
}

} // namespace nxt::engine
