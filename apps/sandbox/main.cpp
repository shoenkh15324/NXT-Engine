#include <cstdio>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>
#include <nxt/platform/backends/windows/win32_window.hpp>

namespace {

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
    while (window->isOpen()) {
        window->pollEvents();
    }
    NXT_LOG_INFO(Platform, Window, "window closed");

    return 0;
}
