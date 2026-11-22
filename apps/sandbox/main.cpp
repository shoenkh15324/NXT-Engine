#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/engine/engine.hpp>
#include <nxt/math/aabb.hpp>
#include <nxt/math/mat4.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>
#include <nxt/renderer/world/world.hpp>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

namespace engine = ::nxt::engine;
namespace math = ::nxt::math;
namespace renderer = ::nxt::renderer;

/// @brief --objects를 주지 않았을 때의 기본 오브젝트 수이다.
constexpr std::uint32_t kDefaultObjectCount = 100'000;

/// @brief 명령행 옵션이다.
struct CommandLineOptions {
    /// @brief 만들 오브젝트 수이다.
    std::uint32_t objectCount = kDefaultObjectCount;

    /// @brief 프러스텀 컬링을 켤지 여부이다.
    bool culling = true;
};

/**
 * @brief 명령행에서 --objects N과 --culling on|off를 읽는다.
 *
 * 잘못된 값이나 모르는 인자는 오류로 처리해, 벤치마크가 조용히 다른 설정으로
 * 도는 일을 막는다.
 *
 * @param[in]  argc    인자 수.
 * @param[in]  argv    인자 배열.
 * @param[out] options 읽은 값. 인자가 없으면 기본값이다.
 * @return 파싱에 성공하면 true.
 */
bool parseArguments(int argc, char* argv[], CommandLineOptions& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};

        if (argument == "--objects") {
            if ((index + 1) >= argc) {
                std::fprintf(stderr, "--objects requires a value\n");
                return false;
            }

            const std::string_view text{argv[++index]};
            std::uint32_t parsed = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);

            // from_chars는 부호를 받아들이므로 "-1"이 아주 큰 값으로 감싸인다. 빈 문자열과
            // 함께 먼저 걸러 1 이상의 십진수만 통과시킨다.
            const bool invalid = text.empty() || (text.front() == '-') || (error != std::errc{}) ||
                                 (end != text.data() + text.size()) || (parsed == 0);
            if (invalid) {
                std::fprintf(stderr, "invalid --objects: %.*s\n", static_cast<int>(text.size()), text.data());
                return false;
            }

            options.objectCount = parsed;
            continue;
        }

        if (argument == "--culling") {
            if ((index + 1) >= argc) {
                std::fprintf(stderr, "--culling requires on|off\n");
                return false;
            }

            const std::string_view text{argv[++index]};
            if (text == "on") {
                options.culling = true;
            } else if (text == "off") {
                options.culling = false;
            } else {
                std::fprintf(stderr, "invalid --culling: %.*s\n", static_cast<int>(text.size()), text.data());
                return false;
            }
            continue;
        }

        std::fprintf(stderr, "unknown argument: %.*s\n", static_cast<int>(argument.size()), argument.data());
        return false;
    }

    return true;
}

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

/**
 * @brief 고정된 그리드에 오브젝트 N개를 만든다.
 *
 * 각 오브젝트는 단위 삼각형 하나와 변환·색만 가진다. 그리드는 clip 공간의
 * [-1.25, 1.25] 범위에 놓아 가장자리 오브젝트가 화면 밖으로 나가게 한다.
 * 1-A4에서 프러스텀 컬링을 붙였을 때 컬링 효과가 0이 되지 않게 하기 위해서다.
 *
 * @param[in,out] world       채울 월드.
 * @param[in]     objectCount 만들 오브젝트 수. 1 이상이어야 한다.
 *
 * @note 같은 objectCount에서 항상 같은 순서와 같은 변환이 나온다. 벤치마크의
 *       재현성은 이 고정 배치에 의존한다.
 */
void buildWorld(renderer::World& world, std::uint32_t objectCount) {
    constexpr math::Aabb kUnitTriangleBounds{
        .min = {-0.5f, -0.5f, 0.0f},
        .max = {0.5f, 0.5f, 0.0f},
    };

    const auto columns = static_cast<std::uint32_t>(std::ceil(std::sqrt(static_cast<float>(objectCount))));
    const float spacing = 2.5f / static_cast<float>(columns);
    const float triangleScale = spacing * 0.9f;
    const float colorDivisor = static_cast<float>(std::max(columns - 1u, 1u));

    for (std::uint32_t index = 0; index < objectCount; ++index) {
        const std::uint32_t column = index % columns;
        const std::uint32_t row = index / columns;

        const float x = -1.25f + ((static_cast<float>(column) + 0.5f) * spacing);
        const float y = -1.25f + ((static_cast<float>(row) + 0.5f) * spacing);

        const math::Mat4 transform = math::translation(math::Vec3{x, y, 0.5}) * math::scale(triangleScale);

        world.add(renderer::WorldObject{
            .transform = transform,
            .bounds = math::transformed(kUnitTriangleBounds, transform),
            .color = {0.25f + (0.75f * static_cast<float>(column) / colorDivisor),
                      0.25f + (0.75f * static_cast<float>(row) / colorDivisor), 0.55f},
        });
    }
}

} // namespace

int main(int argc, char* argv[]) {
    CommandLineOptions options;
    if (!parseArguments(argc, argv, options)) {
        return 1;
    }

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

    // 창 · instance · device · swapchain · 렌더러 · 프레임 루프를 엔진이 조립하고
    // 소유한다. 앱은 셰이더와 창 설정만 넘긴다. 셰이더 배열은 create()가 끝나면
    // 버려도 된다 — 파이프라인을 만드는 동안만 읽는다.
    auto engineInstance = engine::Engine::create(engine::EngineDesc{.appName = "NXT Sandbox",
                                                                    .window = {.title = "NXT Sandbox"},
                                                                    .vertexShader = vertexShader,
                                                                    .fragmentShader = fragmentShader,
                                                                    .culling = options.culling});
    if (!engineInstance) {
        return 1;
    }

    // 앱은 지속 상태인 World만 채운다. GPU 버퍼·파이프라인·프레임 루프는 엔진이
    // 소유하고, 렌더러는 매 프레임 World를 참조로 받는다.
    buildWorld(engineInstance->world(), options.objectCount);
    // 컬링 설정도 함께 남긴다. 같은 objects 수에서 culling on/off를 구분하지 못하면
    // 로그만 보고 어느 설정의 결과인지 판단할 수 없다.
    NXT_LOG_INFO(Renderer, World, "world built: objects={} culling={}", engineInstance->world().objectCount(),
                 options.culling ? "on" : "off");

    // 창이 닫힐 때까지 이벤트 · 프레임 · 리사이즈 · fps 로그를 엔진이 처리한다.
    engineInstance->run();

    return 0;
}
