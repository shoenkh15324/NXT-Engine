#include <nxt/core/diagnostics/log.hpp>
#include <utility>

namespace nxt::log {

namespace {

LogManager* gLogManager = nullptr;

/**
 * sink 하나짜리 span을 만든다.
 * 멤버 초기화 목록에서 std::span을 직접 두 인자로 만들면 함수 스타일 캐스트로
 * 해석돼 컴파일이 깨진다. 별도 함수로 감싸면 타입이 명확해진다.
 */
} // namespace

LogManager::LogManager(std::span<LogSink*> sinks) noexcept : sinks_(sinks) {}

LogManager::LogManager(LogSink& sink) noexcept {
    // span은 배열을 가리켜야 하므로 참조 주소만으로는 만들 수 없다.
    // 멤버 배열에 하나 담아 그 배열을 가리키게 한다.
    singleSinkStorage_[0] = &sink;
    sinks_ = std::span<LogSink*>(singleSinkStorage_, std::size_t{1});
}

void LogManager::write(const LogLevel level, const LogCategory category, std::string message,
                       const std::source_location location) noexcept {
    std::scoped_lock lock(mutex_);
    if (!shouldLog(level, category)) {
        return;
    }

    LogRecord record{
        .level = level,
        .category = category,
        .message = std::move(message),
        .threadId = std::this_thread::get_id(),
        .location = location,
        .timestamp = LogRecord::Clock::now(),
    };

    // 레코드를 하나 만들어 모든 Sink에 넘긴다. Sink별로 다시 포맷하지 않는다.
    for (LogSink* const sink : sinks_) {
        if (sink != nullptr) {
            sink->write(record);
        }
    }
}

void LogManager::setLevel(const LogLevel level) noexcept {
    std::scoped_lock lock(mutex_);
    level_ = level;
}

LogLevel LogManager::level() const noexcept {
    std::scoped_lock lock(mutex_);
    return level_;
}

void LogManager::setCategoryLevel(const LogCategory category, const LogLevel level) noexcept {
    std::scoped_lock lock(mutex_);
    categoryLevels_[static_cast<std::size_t>(category)] = level;
}

void LogManager::resetCategoryLevel(const LogCategory category) noexcept {
    std::scoped_lock lock(mutex_);
    categoryLevels_[static_cast<std::size_t>(category)].reset();
}

LogLevel LogManager::categoryLevel(const LogCategory category) const noexcept {
    std::scoped_lock lock(mutex_);

    const auto categoryIndex = static_cast<std::size_t>(category);
    const auto& categoryLevel = categoryLevels_[categoryIndex];

    if (categoryLevel.has_value()) {
        return *categoryLevel;
    }

    return level_;
}

bool LogManager::shouldLog(const LogLevel level, const LogCategory category) const noexcept {
    const auto categoryIndex = static_cast<std::size_t>(category);
    const auto categoryLevel = categoryLevels_[categoryIndex];
    const auto effectiveLevel = categoryLevel.value_or(level_);
    return level >= effectiveLevel;
}

void setLogManager(LogManager& manager) noexcept {
    gLogManager = &manager;
}

LogManager* logManager() noexcept {
    return gLogManager;
}

void log(const LogLevel level, const LogCategory category, std::string message,
         const std::source_location location) noexcept {
    if (gLogManager == nullptr) {
        return;
    }
    gLogManager->write(level, category, std::move(message), location);
}

std::string_view toString(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
    }
    return "UNKNOWN";
}

std::string_view toString(const LogCategory category) noexcept {
    switch (category) {
        case LogCategory::Core:
            return "Core";
        case LogCategory::Platform:
            return "Platform";
        case LogCategory::Renderer:
            return "Renderer";
        case LogCategory::Graphics:
            return "Graphics";
        case LogCategory::Assets:
            return "Assets";
        case LogCategory::Engine:
            return "Engine";
    }
    return "Unknown";
}

} // namespace nxt::log
