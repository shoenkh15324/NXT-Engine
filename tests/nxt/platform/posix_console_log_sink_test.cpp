#include <doctest/doctest.h>
#include <nxt/platform/backends/linux/log_sink/posix_console_log_sink.hpp>
#include <string>
#include <thread>

namespace {

using nxt::core::log::LogCategory;
using nxt::core::log::LogLayer;
using nxt::core::log::LogLevel;
using nxt::core::log::LogRecord;
using nxt::core::log::LogSubsystem;
using nxt::platform::posix::log::LogColor;
using nxt::platform::posix::log::PosixConsoleLogSink;

LogRecord makeRecord(const LogLevel level, const std::string& message) {
    return LogRecord{
        .level = level,
        .category = LogCategory{LogLayer::Platform, LogSubsystem::Linux},
        .message = message,
        .threadId = std::this_thread::get_id(),
        .location = std::source_location::current(),
        .timestamp = LogRecord::Clock::now(),
    };
}

} // namespace

TEST_CASE("색 없이 조합하면 레벨과 카테고리와 메시지만 남는다") {
    const std::string line = PosixConsoleLogSink::format(makeRecord(LogLevel::Warn, "console message"), false);

    CHECK(line.find("[WARN]") != std::string::npos);
    CHECK(line.find("[Platform][Linux]") != std::string::npos);
    CHECK(line.find("console message") != std::string::npos);
    CHECK(line.find("\033[") == std::string::npos);
    CHECK(line.back() == '\n');
}

TEST_CASE("색을 켜면 레벨 색으로 시작하고 기본색으로 끝난다") {
    const std::string line = PosixConsoleLogSink::format(makeRecord(LogLevel::Error, "colored"), true);

    CHECK(line.rfind("\033[31m", 0) == 0);
    CHECK(line.find("\033[0m") != std::string::npos);
    CHECK(line.find("colored") != std::string::npos);
}

TEST_CASE("콘솔 한 줄은 시:분:초로 시작한다") {
    const std::string line = PosixConsoleLogSink::format(makeRecord(LogLevel::Info, "time"), false);

    // [HH:MM:SS] 형태다.
    REQUIRE(line.size() > 10);
    CHECK(line[0] == '[');
    CHECK(line[3] == ':');
    CHECK(line[6] == ':');
    CHECK(line[9] == ']');
}

TEST_CASE("레벨마다 다른 색을 쓴다") {
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Trace) == LogColor::Grey);
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Debug) == LogColor::Cyan);
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Info) == LogColor::White);
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Warn) == LogColor::Yellow);
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Error) == LogColor::Red);
    CHECK(PosixConsoleLogSink::colorOf(LogLevel::Fatal) == LogColor::BrightRed);
}

TEST_CASE("터미널이 아니면 색을 켜지 않는다") {
    // ctest는 표준 출력을 파이프로 받으므로 isatty()가 거짓이다.
    const PosixConsoleLogSink sink;

    CHECK_FALSE(sink.colorSupportedOnStandardOutput());
    CHECK_FALSE(sink.colorSupportedOnStandardError());
}
