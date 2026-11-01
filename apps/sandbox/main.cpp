#include <chrono>
#include <cstdint>
#include <cstdio>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/core/time/timer.hpp>
#include <nxt/engine/frame_loop.hpp>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>
#include <nxt/platform/backends/windows/win32_window.hpp>
#include <thread>

namespace {

namespace engine = ::nxt::engine;
namespace rhi = ::nxt::graphics::rhi;

/// @brief 0-A에서 화면을 채우는 색이다.
constexpr rhi::ClearColor kClearColor{0.10f, 0.20f, 0.30f, 1.0f};

/**
 * @brief 한 프레임을 기록한다. 0-A에서는 clear만 한다.
 *
 * encoder만 받으므로 Device도 CommandBuffer도 만질 수 없다. 그래서 이 함수를
 * 프레임 루프에 넘겨도 "무엇을 그리는지"가 루프로 새지 않는다 (frame.md §3).
 */
void recordClear(rhi::CommandEncoder& encoder, const engine::FrameTarget& target) {
    encoder.beginRenderPass(rhi::RenderPassDesc{
        .color = target.color, .colorFormat = target.colorFormat, .extent = target.extent, .clearColor = kClearColor});
    encoder.endRenderPass();
}

/**
 * 시작 배너를 표준 출력에 직접 쓴다.
 *
 * 배너는 로그가 아니라 앱의 첫 화면이다. LogSink을 거치면 로그 포맷이 붙어
 * 배너로 인식하기 어렵고, 이 시점에는 로깅 레벨 정책도 아직 정해지지 않았다.
 */
void printBanner() {
    std::puts("============================================================");
    std::printf("NXT Engine\n");
    std::printf("Build Date : %s %s\n", __DATE__, __TIME__);
    std::printf("Version    : %s\n", NXT_VERSION_STRING);
    std::printf("Build Type : %s\n", NXT_BUILD_TYPE);
    std::printf("Platform   : %s\n", NXT_PLATFORM_NAME);
#if defined(_MSC_VER)
    std::printf("Compiler   : MSVC %d.%02d\n", _MSC_VER / 100, _MSC_VER % 100);
#elif defined(__clang__)
    std::printf("Compiler   : Clang %d.%02d\n", __clang_major__, __clang_minor__);
#else
    std::printf("Compiler   : unknown\n");
#endif
    std::puts("============================================================");
}

} // namespace

int main() {
    printBanner();

    // 터미널과 파일에 동시에 기록한다. 둘 다 같은 레코드를 받는다.
    nxt::platform::win32::log::Win32ConsoleLogSink consoleSink;
    nxt::platform::win32::log::Win32FileLogSink fileSink(nxt::platform::win32::log::Win32FileLogSink::defaultPath());

    nxt::core::log::LogSink* sinks[] = {&consoleSink, &fileSink};
    nxt::core::log::LogManager manager(sinks);

    nxt::core::log::setLogManager(manager);

    // 레벨을 직접 지정하지 않는다. LogManager::defaultLevel()이 Debug는 Debug,
    // Release는 Error를 정한다. 여기서 덮어쓰면 배포본까지 조용해지지 않는다.
    // 더 낮게 내려야 하면 setLevel()을 호출하되, 그 판단은 앱의 책임이다.

    NXT_LOG_INFO(Engine, None, "Initializing NXT Engine");

    // Sink 안에서는 로그를 남길 수 없다. LogManager가 sink를 부르는 동안 mutex를
    // 잡고 있어서, sink가 다시 로그를 남기면 같은 mutex를 두 번 잠근다. 그래서
    // "파일이 열렸는가"를 여기서 대신 확인한다. 로그 파일이 비었을 때
    // "sink가 안 열렸다"와 "기록할 게 없었다"를 구분할 수 있어야 한다.
    NXT_LOG_INFO(Core, Diagnostics, "file sink open : {} ({})", fileSink.isOpen(), fileSink.defaultPath().string());

    nxt::platform::window::WindowDesc desc;
    desc.title = "NXT Sandbox";
    auto window = nxt::platform::window::createWindow(desc);
    if (!window) {
        return 1;
    }

    NXT_LOG_INFO(Platform, Window, "window ready : {}x{} \"{}\"", window->width(), window->height(), desc.title);

    // GPU 스파인. 선언 순서가 소유 순서이고, 파괴는 그 역순으로 직접 부른다.
    auto instance = rhi::Instance::create({.appName = "NXT Sandbox"});
    const rhi::Surface surface = instance->createSurface(window->native());
    auto device = instance->createDevice(rhi::DeviceDesc{.surface = surface});
    auto swapchain = rhi::Swapchain::create(*device, rhi::SwapchainDesc{.surface = surface});
    auto frameLoop = engine::FrameLoop::create(*device, *swapchain, &recordClear);

    nxt::core::time::Timer fpsTimer;
    std::uint32_t frames = 0;

    while (window->isOpen()) {
        window->pollEvents();
        if (!window->isOpen()) {
            break;
        }

        // 최소화. 그릴 영역이 없어 acquire가 계속 실패하므로 프레임을 부르지 않는다.
        // pollEvents는 PeekMessage라 여기서 쉬지 않으면 CPU를 100% 쓴다.
        if ((window->width() == 0) || (window->height() == 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        // 창 크기와 swapchain 크기가 다르면 낡은 것이다. fence 대기는 recreate가 한다.
        const rhi::Extent2D extent = swapchain->extent();
        if ((window->width() != extent.width) || (window->height() != extent.height)) {
            frameLoop->recreate();
        }

        // acquire/present가 out-of-date를 돌려주는 경우. 루프는 스스로 고치지 않고
        // 여기서 recreate를 부른다 (frame.md §6).
        if (frameLoop->frame() == engine::FrameLoop::Status::OutOfDate) {
            NXT_LOG_INFO(Engine, FrameLoop, "swapchain out of date: recreating");
            frameLoop->recreate();
        }

        ++frames;
        if (fpsTimer.elapsedMs() >= 1000) {
            NXT_LOG_INFO(Engine, FrameLoop, "fps={}", frames);
            frames = 0;
            fpsTimer.reset();
        }
    }

    // 소유 관계의 역순. frame loop가 레인 자원을, swapchain이 이미지와 view를
    // 소유하고, surface는 instance가 파괴한다 (frame.md §9).
    frameLoop.reset();
    swapchain.reset();
    device.reset();
    instance->destroySurface(surface);
    instance.reset();

    NXT_LOG_INFO(Platform, Window, "window closed");
    return 0;
}
