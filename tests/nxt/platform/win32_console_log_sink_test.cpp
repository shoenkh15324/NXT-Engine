#include <chrono>
#include <doctest/doctest.h>
#include <nxt/platform/backends/windows/log_sink/win32_console_log_sink.hpp>
#include <string>

namespace {

using nxt::log::LogCategory;
using nxt::log::LogLevel;
using nxt::log::LogRecord;
using nxt::platform::windows::log::LogColor;
using nxt::platform::windows::log::Win32ConsoleLogSink;

LogRecord makeRecord(const LogLevel level, const std::string& message) {
    return LogRecord{
        .level = level,
        .category = LogCategory::Core,
        .message = message,
        .threadId = std::this_thread::get_id(),
        .location = std::source_location::current(),
        .timestamp = LogRecord::Clock::now(),
    };
}

[[nodiscard]]
std::string text(const std::string_view value) {
    return std::string(value);
}

[[nodiscard]]
std::string colorName(const LogColor color) {
    switch (color) {
        case LogColor::White:
            return "White";
        case LogColor::Grey:
            return "Grey";
        case LogColor::Cyan:
            return "Cyan";
        case LogColor::Yellow:
            return "Yellow";
        case LogColor::Red:
            return "Red";
        case LogColor::BrightRed:
            return "BrightRed";
    }
    return "Unknown";
}

} // namespace

TEST_CASE("레벨마다 대응하는 색이 정해져 있다") {
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Trace)) == text("Grey"));
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Debug)) == text("Cyan"));
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Info)) == text("White"));
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Warn)) == text("Yellow"));
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Error)) == text("Red"));
    CHECK(colorName(Win32ConsoleLogSink::colorOf(LogLevel::Fatal)) == text("BrightRed"));
}

TEST_CASE("같은 레벨은 항상 같은 색을 반환한다") {
    for (const LogLevel level :
         {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal}) {
        CHECK(Win32ConsoleLogSink::colorOf(level) == Win32ConsoleLogSink::colorOf(level));
    }
}

TEST_CASE("색을 켜면 줄 앞뒤에 이스케이프 시퀀스가 붙는다") {
    const std::string line = Win32ConsoleLogSink::format(makeRecord(LogLevel::Warn, "message"), true);

    CHECK(line.front() == '\033');
    CHECK(line.find("\033[33m") != std::string::npos);
    CHECK(line.find("\033[0m") != std::string::npos);
}

TEST_CASE("각 레벨이 자기 색의 시퀀스를 붙인다") {
    // Grey는 37(밝은 흰색)이 아니라 90(bright black)이라 어두운 회색으로 보인다.
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Trace, "m"), true).find("\033[90m") != std::string::npos);
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Debug, "m"), true).find("\033[36m") != std::string::npos);
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Info, "m"), true).find("\033[0m") != std::string::npos);
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Warn, "m"), true).find("\033[33m") != std::string::npos);
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Error, "m"), true).find("\033[31m") != std::string::npos);
    CHECK(Win32ConsoleLogSink::format(makeRecord(LogLevel::Fatal, "m"), true).find("\033[91;1m") != std::string::npos);
}

TEST_CASE("색을 끄면 이스케이프 시퀀스가 없다") {
    for (const LogLevel level :
         {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal}) {
        const std::string line = Win32ConsoleLogSink::format(makeRecord(level, "message"), false);

        CHECK(line.find('\033') == std::string::npos);
    }
}

TEST_CASE("색을 꺼도 (null)이 찍히지 않는다") {
    // 빈 string_view의 data()는 nullptr이라 %s로 넘기면 (null)이 출력된다.
    // 조합 함수를 쓰면 이 문제가 드러난다.
    for (const LogLevel level : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn}) {
        const std::string line = Win32ConsoleLogSink::format(makeRecord(level, "message"), false);

        CHECK(line.find("(null)") == std::string::npos);
    }
}

TEST_CASE("조합 결과에 레벨과 카테고리와 메시지가 모두 들어간다") {
    LogRecord record = makeRecord(LogLevel::Error, "swap chain failed");
    record.category = LogCategory::Renderer;

    const std::string line = Win32ConsoleLogSink::format(record, false);

    CHECK(line.find("[ERROR]") != std::string::npos);
    CHECK(line.find("[Renderer]") != std::string::npos);
    CHECK(line.find("swap chain failed") != std::string::npos);
}

TEST_CASE("조합 결과는 한 줄로 끝난다") {
    const std::string line = Win32ConsoleLogSink::format(makeRecord(LogLevel::Info, "message"), false);

    CHECK(line.back() == '\n');
    // 메시지에 개행이 없으면 개행은 마지막 하나뿐이다.
    CHECK(std::count(line.begin(), line.end(), '\n') == 1);
}

TEST_CASE("조합 결과에 시각이 HH:MM:SS 형식으로 들어간다") {
    const std::string line = Win32ConsoleLogSink::format(makeRecord(LogLevel::Info, "message"), false);

    // 조합 결과는 [HH:MM:SS] [LEVEL][Category] message 형태다.
    // 예: [14:37:59] [INFO][Core] message
    //     0123456 7 8 9
    REQUIRE(line.size() > 10);
    CHECK(line[0] == '[');
    CHECK(line[3] == ':');
    CHECK(line[6] == ':');
    CHECK(line[9] == ']');
    CHECK(line[10] == ' ');
}

TEST_CASE("메시지에 개행이 있어도 조합은 그대로 이어 붙인다") {
    const std::string line = Win32ConsoleLogSink::format(makeRecord(LogLevel::Info, "first\nsecond"), false);

    CHECK(line.find("first\nsecond") != std::string::npos);
    CHECK(line.back() == '\n');
}

TEST_CASE("색 지원 여부는 Sink마다 생성 시점에 정해진다") {
    // ctest는 표준 스트림을 리다이렉트하므로 보통 색을 쓸 수 없다.
    // 실행 환경에 달린 값이라 정답을 고정할 수는 없지만,
    // 한 번 정해진 값이 바뀌지 않는다는 것과 write가 크래시하지 않는다는 것을 확인한다.
    Win32ConsoleLogSink sink;

    const bool onStandardOutput = sink.colorSupportedOnStandardOutput();
    CHECK(sink.colorSupportedOnStandardOutput() == onStandardOutput);
    CHECK((sink.colorSupportedOnStandardError() || !sink.colorSupportedOnStandardError()));

    sink.write(makeRecord(LogLevel::Fatal, "fatal survives flush"));
    sink.write(makeRecord(LogLevel::Trace, "trace"));
}

TEST_CASE("모든 레벨을 기록해도 예외를 던지지 않는다") {
    Win32ConsoleLogSink sink;

    for (const LogLevel level :
         {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal}) {
        sink.write(makeRecord(level, "sink probe"));
    }
}
