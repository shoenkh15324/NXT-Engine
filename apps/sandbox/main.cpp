#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/core/time/timer.hpp>
#include <nxt/engine/frame_loop.hpp>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>
#include <nxt/platform/backends/windows/win32_window.hpp>
#include <nxt/renderer/mesh/mesh.hpp>
#include <nxt/renderer/passes/triangle_pass.hpp>
#include <thread>
#include <vector>

namespace {

namespace engine = ::nxt::engine;
namespace renderer = ::nxt::renderer;
namespace rhi = ::nxt::graphics::rhi;

/// @brief 0-A에서 화면을 채우는 색이다.
constexpr rhi::ClearColor kClearColor{0.10f, 0.20f, 0.30f, 1.0f};

/**
 * @brief SPIR-V 파일을 32비트 워드 배열로 읽는다.
 *
 * createShader()는 std::span<const std::uint32_t>를 받으므로 바이트 배열이 아니라 워드
 * 단위로 읽는다. 파일 크기가 4의 배수가 아니면 SPIR-V가 아니므로 실패로 본다.
 *
 * @param[in] path 읽을 .spv 파일 경로.
 * @return 읽은 워드 배열. 실패하면 비어 있다.
 */
std::vector<std::uint32_t> loadSpirv(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        NXT_LOG_ERROR(Renderer, Passes, "shader open failed: {}", path.string());
        return {};
    }

    const std::streamsize byteSize = file.tellg();
    if ((byteSize <= 0) || ((byteSize % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0)) {
        NXT_LOG_ERROR(Renderer, Passes, "shader size is not a multiple of 4: {}", path.string());
        return {};
    }

    std::vector<std::uint32_t> words(static_cast<std::size_t>(byteSize) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()), byteSize);
    if (!file) {
        NXT_LOG_ERROR(Renderer, Passes, "shader read failed: {}", path.string());
        return {};
    }

    NXT_LOG_INFO(Renderer, Passes, "shader loaded: {} ({} words)", path.filename().string(), words.size());
    return words;
}

/**
 * @brief 한 프레임을 기록한다. 0-B에서는 삼각형을 그린다.
 *
 * encoder만 받으므로 Device도 CommandBuffer도 만질 수 없다. 그래서 이 함수를
 * 프레임 루프에 넘겨도 "무엇을 그리는지"가 루프로 새지 않는다 (frame.md §3).
 *
 * @param[in]     pass    삼각형을 그리는 pass. 프레임 루프보다 오래 산다.
 * @param[in,out] encoder 기록 중인 command buffer의 창구.
 * @param[in]     target  부착물 · 크기 · clear 값.
 */
void recordScene(const renderer::TrianglePass& pass, rhi::CommandEncoder& encoder, const engine::FrameTarget& target) {
    pass.record(encoder, rhi::RenderPassDesc{.color = target.color,
                                             .colorFormat = target.colorFormat,
                                             .extent = target.extent,
                                             .clearColor = kClearColor,
                                             .depth = target.depth,
                                             .depthFormat = target.depthFormat});
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

    // CMake가 실행 디렉터리 옆 shaders/로 복사한 SPIR-V를 읽는다. GPU 객체를 만들기
    // 전에 읽으므로, 실패하면 자원을 정리하지 않고 바로 끝낼 수 있다.
    const std::filesystem::path shaderDir{NXT_SHADER_DIR};
    const std::vector<std::uint32_t> vertexShader = loadSpirv(shaderDir / "triangle.vert.spv");
    const std::vector<std::uint32_t> fragmentShader = loadSpirv(shaderDir / "triangle.frag.spv");
    if (vertexShader.empty() || fragmentShader.empty()) {
        NXT_LOG_ERROR(Engine, None, "shader load failed: {}", shaderDir.string());
        return 1;
    }

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

    // 파이프라인이 색 형식과 깊이 형식을 알아야 하므로 swapchain 뒤에 만든다.
    // 깊이 형식은 프레임 루프가 이미지를 만들 때 쓰는 값과 같아야 한다.
    auto pass = std::make_unique<renderer::TrianglePass>(*device, swapchain->format(), engine::FrameLoop::kDepthFormat,
                                                         vertexShader, fragmentShader);

    // 두 삼각형은 화면에서 완전히 겹친다. 가까운 쪽(z=0.25)을 먼저 그리고 먼 쪽
    // (z=0.75)을 나중에 그린다. 깊이 테스트가 꺼져 있으면 나중에 그린 초록색이
    // 덮어쓰고, 켜져 있으면 빨간색이 남는다 — B6의 깊이 검증이 이 순서에 달려 있다.
    const std::array<renderer::Mesh, 2> meshes{
        renderer::Mesh{.vertices = {{.position = {-0.5f, -0.5f, 0.25f}, .color = {0.90f, 0.25f, 0.20f}},
                                    {.position = {0.5f, -0.5f, 0.25f}, .color = {0.90f, 0.25f, 0.20f}},
                                    {.position = {0.0f, 0.6f, 0.25f}, .color = {0.90f, 0.25f, 0.20f}}}},
        renderer::Mesh{.vertices = {{.position = {-0.5f, -0.5f, 0.75f}, .color = {0.20f, 0.80f, 0.30f}},
                                    {.position = {0.5f, -0.5f, 0.75f}, .color = {0.20f, 0.80f, 0.30f}},
                                    {.position = {0.0f, 0.6f, 0.75f}, .color = {0.20f, 0.80f, 0.30f}}}},
    };
    pass->upload(meshes);

    // pass는 프레임 루프보다 오래 살아야 한다. 람다가 참조로 잡기 때문이다.
    auto frameLoop = engine::FrameLoop::create(
        *device, *swapchain, [&pass](rhi::CommandEncoder& encoder, const engine::FrameTarget& target) {
            recordScene(*pass, encoder, target);
        });

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
    pass.reset();
    swapchain.reset();
    device.reset();
    instance->destroySurface(surface);
    instance.reset();

    NXT_LOG_INFO(Platform, Window, "window closed");
    return 0;
}
