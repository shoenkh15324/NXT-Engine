#include <chrono>
#include <cstdio>
#include <ctime>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <string>
#include <string_view>

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace nxt::platform::windows::log {

using namespace nxt::core::log;

namespace {

/**
 * 콘솔이 VT 시퀀스를 해석하지 않으면 이스케이프 코드가 그대로 화면에 나온다.
 * Windows 10 1607 이상 콘솔은 기본으로 켜져 있지만, 리다이렉트된 핸들이거나
 * 구형 콘솔 호스트에서는 꺼져 있으므로 핸들마다 한 번 확인한다.
 *
 * @param stdHandle STD_OUTPUT_HANDLE 또는 STD_ERROR_HANDLE.
 *
 * @return VT 시퀀스 처리가 켜져 있으면 true.
 */
[[nodiscard]]
bool enableVirtualTerminalProcessing(const DWORD stdHandle) noexcept {
    const HANDLE handle = ::GetStdHandle(stdHandle);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        return true;
    }

    return ::SetConsoleMode(handle, (mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) != 0;
}

[[nodiscard]]
std::string_view ansiSequence(const LogColor color) noexcept {
    switch (color) {
        case LogColor::White:
            return "\033[0m";
        case LogColor::Grey:
            return "\033[90m";
        case LogColor::Cyan:
            return "\033[36m";
        case LogColor::Yellow:
            return "\033[33m";
        case LogColor::Red:
            return "\033[31m";
        case LogColor::BrightRed:
            return "\033[91;1m";
    }
    return "\033[0m"; // White
}

/**
 * 콘솔에는 시:분:초만 남긴다.
 * 연월일은 파일 Sink가 담당한다. 콘솔에서 날짜까지 보면 한 줄이 너무 길어진다.
 * 밀리초는 사람이 읽기에 쓸모가 있어 초 단위까지만 남긴다.
 */
[[nodiscard]]
std::tm toLocalTime(const LogRecord::Clock::time_point point) noexcept {
    const std::time_t seconds = LogRecord::Clock::to_time_t(point);
    std::tm result{};
    ::localtime_s(&result, &seconds);
    return result;
}

} // namespace

Win32ConsoleLogSink::Win32ConsoleLogSink() noexcept
    : colorOnStandardOutput_(enableVirtualTerminalProcessing(STD_OUTPUT_HANDLE)),
      colorOnStandardError_(enableVirtualTerminalProcessing(STD_ERROR_HANDLE)) {}

LogColor Win32ConsoleLogSink::colorOf(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:
            return LogColor::Grey;
        case LogLevel::Debug:
            return LogColor::Cyan;
        case LogLevel::Info:
            return LogColor::White;
        case LogLevel::Warn:
            return LogColor::Yellow;
        case LogLevel::Error:
            return LogColor::Red;
        case LogLevel::Fatal:
            return LogColor::BrightRed;
    }
    return LogColor::White;
}

bool Win32ConsoleLogSink::colorSupportedOnStandardOutput() const noexcept {
    return colorOnStandardOutput_;
}

bool Win32ConsoleLogSink::colorSupportedOnStandardError() const noexcept {
    return colorOnStandardError_;
}

std::FILE* Win32ConsoleLogSink::streamFor(const LogLevel level) const noexcept {
    // Error 이상은 표준 오류로 분리해 셸 리다이렉션으로도 구분되게 한다.
    return (level >= LogLevel::Error) ? stderr : stdout;
}

std::string Win32ConsoleLogSink::format(const LogRecord& record, const bool useColor) {
    const std::tm time = toLocalTime(record.timestamp);
    char timeText[32];
    const int written =
        std::snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", time.tm_hour, time.tm_min, time.tm_sec);
    if (written <= 0) {
        return {};
    }

    std::string line;
    if (useColor) {
        line += ansiSequence(colorOf(record.level));
    }

    line += '[';
    line += timeText;
    line += "] [";
    line += toString(record.level);
    line += "][";
    line += toString(record.category);
    line += "] ";
    line += record.message;

    if (useColor) {
        line += ansiSequence(LogColor::White);
    }

    line += '\n';
    return line;
}

void Win32ConsoleLogSink::write(const LogRecord& record) noexcept {
    std::FILE* const stream = streamFor(record.level);
    if (stream == nullptr) {
        return;
    }

    const bool useColor = (stream == stderr) ? colorOnStandardError_ : colorOnStandardOutput_;

    std::fputs(format(record, useColor).c_str(), stream);

    // Fatal은 버퍼에 남으면 비정상 종료 시 유실된다.
    if (record.level == LogLevel::Fatal) {
        std::fflush(stream);
    }
}

} // namespace nxt::platform::windows::log
