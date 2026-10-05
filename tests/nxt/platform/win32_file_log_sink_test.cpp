#include <doctest/doctest.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>
#include <string>

namespace {

using nxt::log::LogCategory;
using nxt::log::LogLevel;
using nxt::log::LogRecord;
using nxt::platform::windows::log::FileLogBufferPolicy;
using nxt::platform::windows::log::Win32FileLogSink;

[[nodiscard]]
std::tm localNow() {
    const auto seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    ::localtime_s(&local, &seconds);
    return local;
}

LogRecord makeRecord(const LogLevel level, const std::string& message, const int line = 100) {
    return LogRecord{
        .level = level,
        .category = LogCategory::Core,
        .message = message,
        .threadId = std::this_thread::get_id(),
        .location = std::source_location::current(),
        .timestamp = LogRecord::Clock::now(),
    };
}

/**
 * 각 테스트는 자기만의 임시 디렉터리를 쓰고 끝나면 지운다.
 * 테스트 프로세스 위치에 logs/를 만들면 산출물이 리포에 남는다.
 */
class TempLogDir {
public:
    explicit TempLogDir(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / ("nxt_log_sink_" + name)) {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    ~TempLogDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TempLogDir(const TempLogDir&) = delete;
    TempLogDir& operator=(const TempLogDir&) = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept {
        return path_;
    }

    [[nodiscard]]
    std::filesystem::path file() const {
        return path_ / "nxt.log";
    }

    /**
     * 파일 전체 내용을 읽는다.
     * sink가 아직 살아 있어도 파일 스트림 버퍼를 확인하므로 호출 전에 flush가 필요할 수 있다.
     */
    [[nodiscard]]
    std::string readAll() const {
        std::ifstream stream(file(), std::ios::in | std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]]
    bool fileExists() const {
        std::error_code ignored;
        return std::filesystem::exists(file(), ignored);
    }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("상위 디렉터리가 없으면 만들어 연다") {
    TempLogDir dir("create_dirs");

    {
        Win32FileLogSink sink(dir.file());
        CHECK(sink.isOpen());
    }

    CHECK(dir.fileExists());
}

TEST_CASE("지정한 경로로 파일을 연다") {
    TempLogDir dir("open");

    Win32FileLogSink sink(dir.file());

    CHECK(sink.isOpen());
}

TEST_CASE("defaultPath는 logs 디렉터리 아래에 만든다") {
    const std::filesystem::path path = Win32FileLogSink::defaultPath();

    CHECK(path.parent_path().filename() == "logs");
    CHECK(path.extension() == ".log");
    CHECK(path.filename().string().rfind("nxt_", 0) == 0);
}

TEST_CASE("한 줄에 시각과 레벨과 카테고리가 기록된다") {
    TempLogDir dir("fields");

    {
        Win32FileLogSink sink(dir.file());
        sink.write(makeRecord(LogLevel::Warn, "sink writes message"));
    }

    const std::string content = dir.readAll();

    CHECK(content.find("2026") != std::string::npos);
    CHECK(content.find("[WARN]") != std::string::npos);
    CHECK(content.find("[Core]") != std::string::npos);
    CHECK(content.find("sink writes message") != std::string::npos);
    CHECK(content.back() == '\n');
}

TEST_CASE("파일은 호출 위치를 함께 기록한다") {
    TempLogDir dir("location");

    // makeRecord가 source_location을 내부에서 만들기 때문에 라인 번호는
    // makeRecord 호출 지점이 아니라 LogRecord 생성 지점이 된다.
    // 여기서는 정확한 라인 값 대신 파일명이 찍히고 라인 번호가 남는지만 확인한다.
    {
        Win32FileLogSink sink(dir.file());
        sink.write(makeRecord(LogLevel::Info, "with location"));
    }

    const std::string content = dir.readAll();

    CHECK(content.find("win32_file_log_sink_test.cpp") != std::string::npos);
    CHECK(content.find(".cpp:") != std::string::npos);
}

TEST_CASE("임계값 미만이면 소멸 전까지 파일에 없다") {
    TempLogDir dir("buffered");

    {
        Win32FileLogSink sink(dir.file());
        sink.write(makeRecord(LogLevel::Info, "still buffered"));
        // 버퍼가 차지 않았으니 파일은 비어 있다.
        CHECK(dir.readAll().empty());
    }

    CHECK(dir.readAll().find("still buffered") != std::string::npos);
}

TEST_CASE("임계값을 넘기면 파일로 넘기고 버퍼를 비운다") {
    TempLogDir dir("threshold");

    FileLogBufferPolicy policy;
    policy.capacity = 1024;
    policy.flushThreshold = 256;

    Win32FileLogSink sink(dir.file(), policy);

    // 첫 줄이 임계값을 넘도록 긴 메시지를 반복해서 기록한다.
    const std::string longMessage(200, 'x');
    for (int i = 0; i < 4; ++i) {
        sink.write(makeRecord(LogLevel::Info, longMessage));
    }

    // 버퍼를 넘겼으므로 앞선 줄이 이미 파일에 있다.
    const std::string content = dir.readAll();
    CHECK_FALSE(content.empty());
    CHECK(content.find(longMessage) != std::string::npos);
}

TEST_CASE("Fatal은 소멸 전에도 쌓였던 로그와 함께 기록된다") {
    TempLogDir dir("fatal_flush");

    Win32FileLogSink sink(dir.file(), FileLogBufferPolicy{.capacity = 4096, .flushThreshold = 4096});

    sink.write(makeRecord(LogLevel::Info, "before fatal"));
    CHECK(dir.readAll().empty());

    sink.write(makeRecord(LogLevel::Fatal, "fatal message"));

    // Fatal은 버퍼를 비우며 쓰므로 소멸 전에 바로 읽을 수 있다.
    const std::string content = dir.readAll();
    CHECK(content.find("before fatal") != std::string::npos);
    CHECK(content.find("fatal message") != std::string::npos);
}

TEST_CASE("Fatal 다음에도 기록은 계속 동작한다") {
    TempLogDir dir("after_fatal");

    {
        Win32FileLogSink sink(dir.file(), FileLogBufferPolicy{.capacity = 4096, .flushThreshold = 4096});
        sink.write(makeRecord(LogLevel::Fatal, "fatal message"));
        sink.write(makeRecord(LogLevel::Info, "after fatal"));
    }

    const std::string content = dir.readAll();

    CHECK(content.find("fatal message") != std::string::npos);
    CHECK(content.find("after fatal") != std::string::npos);
}

TEST_CASE("버퍼보다 긴 줄도 잘리지 않고 기록된다") {
    TempLogDir dir("oversized");

    FileLogBufferPolicy policy;
    policy.capacity = 64;
    policy.flushThreshold = 64;

    const std::string longMessage(500, 'y');

    {
        Win32FileLogSink sink(dir.file(), policy);
        sink.write(makeRecord(LogLevel::Info, longMessage));
    }

    const std::string content = dir.readAll();

    CHECK(content.find(longMessage) != std::string::npos);
}

TEST_CASE("짧은 줄이 여러 개 쌓이면 순서가 유지된다") {
    TempLogDir dir("order");

    FileLogBufferPolicy policy;
    policy.capacity = 4096;
    policy.flushThreshold = 4096;

    {
        Win32FileLogSink sink(dir.file(), policy);
        for (int i = 0; i < 5; ++i) {
            sink.write(makeRecord(LogLevel::Info, "line " + std::to_string(i)));
        }
    }

    const std::string content = dir.readAll();

    std::size_t previous = 0;
    for (int i = 0; i < 5; ++i) {
        const std::size_t position = content.find("line " + std::to_string(i));
        REQUIRE(position != std::string::npos);
        CHECK(position > previous);
        previous = position;
    }
}

TEST_CASE("임계값이 capacity를 넘으면 capacity로 보정된다") {
    FileLogBufferPolicy policy;
    policy.capacity = 128;
    policy.flushThreshold = 9999;

    policy.normalize();

    CHECK(policy.flushThreshold == 128);
}

TEST_CASE("임계값이 0이면 capacity로 보정된다") {
    FileLogBufferPolicy policy;
    policy.capacity = 256;
    policy.flushThreshold = 0;

    policy.normalize();

    CHECK(policy.flushThreshold == 256);
}

TEST_CASE("주입한 버퍼 크기가 그대로 적용된다") {
    TempLogDir dir("policy");

    FileLogBufferPolicy policy;
    policy.capacity = 32;
    policy.flushThreshold = 32;

    Win32FileLogSink sink(dir.file(), policy);

    // 임계치가 32이므로 첫 줄부터 파일로 넘어간다.
    sink.write(makeRecord(LogLevel::Info, "small capacity"));

    CHECK(dir.readAll().find("small capacity") != std::string::npos);
}

TEST_CASE("열 수 없는 경로면 조용히 아무것도 쓰지 않는다") {
    // 디렉터리를 만들어 두는 구현이므로 열 실패를 재현하려면 잘못된 경로를 준다.
    // Windows에서 콜론은 드라이브 구분자라 파일명 중간에 쓸 수 없다.
    TempLogDir dir("invalid");
    const std::filesystem::path invalid = dir.path() / "a:b" / "log.txt";

    Win32FileLogSink sink(invalid);

    CHECK_FALSE(sink.isOpen());
    sink.write(makeRecord(LogLevel::Fatal, "dropped silently"));
}

TEST_CASE("상위 디렉터리를 이미 만들어 두면 그것을 그대로 쓴다") {
    TempLogDir dir("existing");

    std::error_code ignored;
    std::filesystem::create_directories(dir.path() / "logs");

    Win32FileLogSink sink(dir.path() / "logs" / "nxt.log");

    CHECK(sink.isOpen());
    CHECK(std::filesystem::is_directory(dir.path()));
}

TEST_CASE("파일은 열 때 기존 내용을 지운다") {
    TempLogDir dir("truncate");

    {
        Win32FileLogSink first(dir.file());
        first.write(makeRecord(LogLevel::Info, "first run"));
    }
    REQUIRE(dir.readAll().find("first run") != std::string::npos);

    {
        // 같은 경로로 다시 열면 이전 실행의 로그가 사라진다.
        Win32FileLogSink second(dir.file());
        second.write(makeRecord(LogLevel::Info, "second run"));
    }

    const std::string content = dir.readAll();

    CHECK(content.find("first run") == std::string::npos);
    CHECK(content.find("second run") != std::string::npos);
}

TEST_CASE("defaultPath는 실행 시각이 붙은 파일명을 만든다") {
    const std::string fileName = Win32FileLogSink::defaultPath().filename().string();

    // nxt_YYYYMMDD_HHMMSS_mmm.log 형태다.
    //   0123 45678901 2 345678 9 012 3456
    REQUIRE(fileName.size() == 27);

    CHECK(fileName.substr(0, 4) == "nxt_");
    CHECK(fileName[12] == '_');
    CHECK(fileName[19] == '_');
    CHECK(fileName.substr(23, 4) == ".log");

    // 시각 부분은 숫자만 온다.
    for (std::size_t i = 4; i < 23; ++i) {
        if (i == 12 || i == 19) {
            continue;
        }
        CHECK((fileName[i] >= '0' && fileName[i] <= '9'));
    }
}

TEST_CASE("defaultPath의 날짜 부분은 오늘 날짜와 일치한다") {
    const std::string fileName = Win32FileLogSink::defaultPath().filename().string();

    // 파일명은 nxt_YYYYMMDD_HHMMSS_mmm.log 형태다.
    // 인덱스 4~11이 YYYYMMDD, 12가 '_', 13~18이 시분초다.
    const std::tm now = localNow();

    CHECK(fileName.substr(4, 8) == std::format("{:04}{:02}{:02}", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday));
    CHECK(fileName.substr(13, 6) == std::format("{:02}{:02}{:02}", now.tm_hour, now.tm_min, now.tm_sec));
}
