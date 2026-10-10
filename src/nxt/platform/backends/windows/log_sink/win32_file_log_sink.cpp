#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <nxt/platform/backends/windows/log_sink/win32_file_log_sink.hpp>

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace nxt::platform::win32::log {

using namespace nxt::core::log;

namespace {

/**
 * 로그는 사람이 읽는 기록이므로 연월일과 밀리초를 함께 남긴다.
 * 로케일에 의존하지 않도록 숫자를 직접 채운다.
 */
[[nodiscard]]
std::string formatTimestamp(const LogRecord::Clock::time_point point) {
    const auto secondsSinceEpoch = LogRecord::Clock::to_time_t(point);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(point.time_since_epoch()).count() % 1000;

    std::tm local{};
    if (::localtime_s(&local, &secondsSinceEpoch) != 0) {
        return {};
    }

    char text[32];
    const int written =
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d.%03d", local.tm_year + 1900, local.tm_mon + 1,
                      local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, static_cast<int>(milliseconds));
    if (written <= 0) {
        return {};
    }

    return std::string(text, static_cast<std::size_t>(written));
}

/**
 * 로그 파일은 사람이 직접 찾아보는 곳이라 호출 지점을 함께 남긴다.
 * file_name()은 컴파일러가 넘긴 값이라 보통 전체 경로다.
 */
[[nodiscard]]
std::string formatLocation(const LogRecord& record) {
    if (record.location.line() <= 0) {
        return {};
    }
    return std::string(record.location.file_name()) + ':' + std::to_string(record.location.line());
}

/**
 * 로그 파일명에 쓸 실행 시각을 만든다.
 *
 * 밀리초까지 넣는다. 초까지만 넣으면 같은 초에 프로그램을 두 번 실행했을 때
 * 두 번째가 첫 번째 로그를 덮어써서 크래시 원인을 잃는다.
 * 로케일에 의존하지 않도록 숫자를 직접 채운다.
 */
[[nodiscard]]
std::string formatFileStem(const LogRecord::Clock::time_point point) {
    const auto secondsSinceEpoch = LogRecord::Clock::to_time_t(point);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(point.time_since_epoch()).count() % 1000;

    std::tm local{};
    if (::localtime_s(&local, &secondsSinceEpoch) != 0) {
        return "nxt_unknown";
    }

    char text[48];
    const int written =
        std::snprintf(text, sizeof(text), "nxt_%04d%02d%02d_%02d%02d%02d_%03d", local.tm_year + 1900, local.tm_mon + 1,
                      local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, static_cast<int>(milliseconds));
    if (written <= 0) {
        return "nxt_unknown";
    }

    return std::string(text, static_cast<std::size_t>(written));
}

} // namespace

Win32FileLogSink::Win32FileLogSink(const std::filesystem::path& path) : Win32FileLogSink(path, FileLogBufferPolicy{}) {}

Win32FileLogSink::Win32FileLogSink(const std::filesystem::path& path, FileLogBufferPolicy policy) {
    policy.normalize();

    buffer_.resize(policy.capacity);
    flushThreshold_ = policy.flushThreshold;

    // 디렉터리를 열기 전에 만든다. 순서가 바뀌면 첫 실행이 조용히 실패한다.
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);

    // 파일명에 실행 시각이 들어가 보통 겹치지 않지만, 같은 밀리초에 두 번 실행하면
    // 겹칠 수 있다. 그때는 기존 내용을 지워 한 실행의 로그가 섞이지 않게 한다.
    stream_.open(path, std::ios::out | std::ios::trunc);
}

Win32FileLogSink::~Win32FileLogSink() {
    flushBuffer();
    commit();
}

std::filesystem::path Win32FileLogSink::defaultPath() {
    // 파일명 시각은 프로그램 시작 시각으로 잡는다.
    // 파일명 하나만 봐도 어느 실행의 로그인지 결정돼야 한다.
    const std::string fileName = formatFileStem(LogRecord::Clock::now()) + ".log";

    std::wstring buffer(MAX_PATH, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

    if (length == 0 || length >= buffer.size()) {
        return std::filesystem::current_path() / "logs" / fileName;
    }

    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path() / "logs" / fileName;
}

bool Win32FileLogSink::isOpen() const noexcept {
    return stream_.is_open();
}

void Win32FileLogSink::write(const LogRecord& record) noexcept {
    if (!stream_.is_open()) {
        return;
    }

    const std::string timestamp = formatTimestamp(record.timestamp);
    if (timestamp.empty()) {
        return;
    }

    const std::string location = formatLocation(record);

    std::string line;
    line.reserve(location.size() + record.message.size() + 64);
    line += '[';
    line += timestamp;
    line += "]";
    if (!location.empty()) {
        line += ' ';
        line += location;
        line += ' ';
    }
    line += "[";
    line += toString(record.level);
    line += ']';
    appendCategoryGroups(line, record.category);
    line += ' ';
    line += record.message;
    line += '\n';

    append(line);

    // Fatal은 지금까지 쌓은 내용과 함께 즉시 디스크에 확정한다.
    // 버퍼를 비우지 않고 Fatal만 밀어내면 직전 로그가 유실된다.
    if (record.level == LogLevel::Fatal) {
        flushBuffer();
        commit();
    }
}

void Win32FileLogSink::append(const std::string& line) noexcept {
    // 남은 공간보다 길면 먼저 비운다. 일부만 넣으면 줄이 깨진다.
    if (line.size() > buffer_.size() - buffered_) {
        flushBuffer();
        // 버퍼 전체보다 긴 줄은 버퍼를 거치지 않고 바로 쓴다.
        if (line.size() > buffer_.size()) {
            stream_.write(line.data(), static_cast<std::streamsize>(line.size()));
            commit();
            return;
        }
    }

    std::copy_n(line.begin(), line.size(), buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
    buffered_ += line.size();

    if (buffered_ >= flushThreshold_) {
        flushBuffer();
    }
}

void Win32FileLogSink::flushBuffer() noexcept {
    if (buffered_ == 0) {
        return;
    }

    stream_.write(buffer_.data(), static_cast<std::streamsize>(buffered_));
    buffered_ = 0;

    // 파일 스트림 버퍼를 함께 비운다. 안 비우면 버퍼를 넘겼어도 디스크 반영이
    // ofstream 내부 버퍼가 찰 때까지 밀려 비정상 종료 시 그만큼 유실된다.
    commit();
}

void Win32FileLogSink::commit() noexcept {
    if (!stream_.is_open()) {
        return;
    }
    stream_.flush();
}

} // namespace nxt::platform::win32::log
