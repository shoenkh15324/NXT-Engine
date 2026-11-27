#include <chrono>
#include <cstdio>
#include <ctime>
#include <nxt/platform/backends/linux/log_sink/posix_console_log_sink.hpp>
#include <string>
#include <string_view>
#include <unistd.h>

namespace nxt::platform::posix::log {

using namespace nxt::core::log;

namespace {

/**
 * 리다이렉트된 스트림에 이스케이프 코드를 쓰면 로그 파일에 제어 문자가 남는다.
 * isatty()로 터미널일 때만 색상을 켠다.
 *
 * @param stream 표준 출력 또는 표준 오류.
 *
 * @return 터미널이면 true.
 */
[[nodiscard]]
bool isTerminal(std::FILE* stream) noexcept {
    if (stream == nullptr) {
        return false;
    }
    return ::isatty(::fileno(stream)) != 0;
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
    ::localtime_r(&seconds, &result);
    return result;
}

} // namespace

PosixConsoleLogSink::PosixConsoleLogSink() noexcept
    : colorOnStandardOutput_(isTerminal(stdout)), colorOnStandardError_(isTerminal(stderr)) {}

LogColor PosixConsoleLogSink::colorOf(const LogLevel level) noexcept {
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

bool PosixConsoleLogSink::colorSupportedOnStandardOutput() const noexcept {
    return colorOnStandardOutput_;
}

bool PosixConsoleLogSink::colorSupportedOnStandardError() const noexcept {
    return colorOnStandardError_;
}

std::FILE* PosixConsoleLogSink::streamFor(const LogLevel level) const noexcept {
    // Error 이상은 표준 오류로 분리해 셸 리다이렉션으로도 구분되게 한다.
    return (level >= LogLevel::Error) ? stderr : stdout;
}

std::string PosixConsoleLogSink::format(const LogRecord& record, const bool useColor) {
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
    line += ']';
    appendCategoryGroups(line, record.category);
    line += ' ';
    line += record.message;

    if (useColor) {
        line += ansiSequence(LogColor::White);
    }

    line += '\n';
    return line;
}

void PosixConsoleLogSink::write(const LogRecord& record) noexcept {
    std::FILE* const stream = streamFor(record.level);
    if (stream == nullptr) {
        return;
    }

    const bool useColor = (stream == stderr) ? colorOnStandardError_ : colorOnStandardOutput_;

    std::fputs(format(record, useColor).c_str(), stream);

    // Fatal은 버퍼에 남으면 비정상 종료 시 유실된다.
    // 터미널이 아니면(파일·파이프로 리다이렉트) 줄 단위로 비운다. 표준 출력은
    // 리다이렉트될 때 완전 버퍼링이라, 프로세스가 신호로 끝나면 마지막 버퍼가
    // 통째로 유실되기 때문이다. CI 로그에서 원인을 잃지 않게 한다.
    if ((record.level == LogLevel::Fatal) || !useColor) {
        std::fflush(stream);
    }
}

} // namespace nxt::platform::posix::log
