#include <cstdio>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>

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
    nxt::platform::windows::log::Win32ConsoleLogSink consoleSink;
    nxt::platform::windows::log::Win32FileLogSink fileSink(
        nxt::platform::windows::log::Win32FileLogSink::defaultPath());

    nxt::log::LogSink* sinks[] = {&consoleSink, &fileSink};
    nxt::log::LogManager manager(sinks);

    nxt::log::setLogManager(manager);
    manager.setLevel(nxt::log::LogLevel::Trace);

    NXT_LOG_INFO(Engine, "Initializing NXT Engine");
    NXT_LOG_INFO(Core, "log file : {}", fileSink.defaultPath().string());

    return 0;
}
