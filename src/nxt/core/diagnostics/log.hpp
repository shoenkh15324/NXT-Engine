#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace nxt::core::log {

/**
 * @brief 로그 메시지의 심각도를 나타낸다.
 *
 * 값이 클수록 심각도가 높다.
 */
enum class LogLevel : std::uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

/**
 * @brief 로그 메시지를 생성한 모듈 계층을 나타낸다.
 *
 * 계층은 src/nxt 아래의 최상위 디렉터리(src/nxt/core, src/nxt/platform 등)에
 * 대응한다.
 */
enum class LogLayer : std::uint8_t {
    Core = 0,
    Platform,
    Graphics,
    Renderer,
    Assets,
    Engine,
};

/**
 * @brief 모듈 바로 아래의 디렉터리를 가리킨다.
 *
 * None은 하위 디렉터리가 없는 모듈 루트 파일(engine/engine.cpp,
 * renderer/renderer.cpp, 앱 main.cpp)에 쓴다.
 *
 * 이름은 계층 간에 겹치지 않는다. 그래서 카테고리별 level override의 배열을
 * LogSubsystem 값만으로 색인해도 정확하다.
 */
enum class LogSubsystem : std::uint8_t {
    None = 0,
    // core
    Diagnostics,
    Handle,
    Time,
    Memory,
    Containers,
    Concurrency,
    Jobs,
    Event,
    // platform
    Window,
    Input,
    Filesystem,
    Win32,
    // graphics
    Rhi,
    Vulkan,
    // renderer
    Graph,
    Material,
    Mesh,
    Passes,
    World,
    // assets
    Types,
    Loaders,
    // engine
    FrameLoop,
    Scene,

    LogSubsystemCount
};

/**
 * @brief 로그 메시지의 출처를 나타낸다.
 *
 * 계층은 항상 있고 하위 시스템은 없을 수 있다. 표시할 때 각각 독립된 대괄호
 * 그룹이 되어, 예로 Platform/None은 "[Platform]", Platform/Win32는
 * "[Platform][Win32]"로 나온다.
 */
struct LogCategory {
    LogLayer layer{LogLayer::Core};
    LogSubsystem subsystem{LogSubsystem::None};

    /// 비교는 defaulted 연산자로부터 파생된다.
    auto operator<=>(const LogCategory&) const noexcept = default;
};

/**
 * @brief 하나의 로그 이벤트에 대한 정보를 소유한다.
 *
 * 로그 메시지는 std::string으로 직접 소유한다.
 * 따라서 향후 비동기 로깅으로 전환하더라도 원본 문자열의 수명에 의존하지 않는다.
 */
struct LogRecord {
    using Clock = std::chrono::system_clock;
    LogLevel level;
    LogCategory category;
    std::string message;
    std::thread::id threadId;
    std::source_location location;
    Clock::time_point timestamp;
};

/**
 * @brief 로그 레코드를 전달받는 출력 대상을 정의한다.
 *
 * LogSink은 로그 메시지를 실제로 출력하거나 저장하는 방법을 추상화한다.
 * Core 로깅 시스템은 로그가 콘솔, 파일, 디버거 또는 기타 저장 장치 중 어디에 기록되는지 알지 않는다.
 */
class LogSink {
public:
    virtual ~LogSink() = default;

    /**
     * @brief 로그 레코드를 출력하거나 저장한다.
     *
     * LogManager는 Sink의 write() 호출을 직렬화한다.
     *
     * @param record 출력할 로그 레코드.
     */
    virtual void write(const LogRecord& record) noexcept = 0;
};

/**
 * @brief 중앙 로깅 관리자를 정의한다.
 *
 * LogManager는 전역 로깅 정책을 관리하고 생성된 로그 레코드를 Sink로 전달한다.
 * LogManager 자체는 싱글턴으로 구현하지 않는다. 전역 접근은 별도의 자유 함수를 통해 제공한다.
 * LogManager는 생성 시 최소 하나의 유효한 LogSink을 요구한다.
 * Sink가 여러 개여도 모든 Sink가 동일한 레코드를 받으며, 어느 출력 장치가 쓰이는지는 모른다.
 */
class LogManager {
public:
    /**
     * @brief 지정된 Sink들을 사용하는 로그 관리자를 생성한다.
     *
     * 모든 Sink는 동일한 레코드를 받는다. Sink가 여러 개이더라도
     * 포맷과 필터링은 한 번만 수행되므로 비용이 선형으로 늘지 않는다.
     *
     * Sink 배열의 수명은 호출자의 책임이다. LogManager는 포인터만 보관한다.
     *
     * @param sinks 로그 레코드를 전달할 출력 대상 목록.
     */
    explicit LogManager(std::span<LogSink*> sinks) noexcept;

    /**
     * @brief Sink 하나로 로그 관리자를 생성한다.
     *
     * @param sink 로그 레코드를 전달할 출력 대상.
     */
    explicit LogManager(LogSink& sink) noexcept;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    /**
     * @brief 로그 메시지를 기록한다.
     *
     * 현재 설정된 전역 및 카테고리별 로그 레벨에 따라 기록 여부를 결정한다.
     * 포맷이 필요하다면 log()이 아니라 NXT_LOG_* 매크로를 사용한다.
     * 로그 시스템은 예외를 외부로 전파하지 않는다.
     *
     * @param level 로그의 심각도.
     * @param category 로그를 생성한 카테고리.
     * @param message 로그 메시지.
     * @param location 로그가 호출된 소스 위치.
     */
    void write(LogLevel level, LogCategory category, std::string message,
               std::source_location location = std::source_location::current()) noexcept;

    /**
     * @brief 전역 최소 로그 레벨을 설정한다.
     *
     * 카테고리에 별도의 레벨이 설정되지 않은 경우 이 레벨이 해당 카테고리의 유효 레벨로 사용된다.
     *
     * @param level 새로운 전역 최소 로그 레벨.
     */
    void setLevel(LogLevel level) noexcept;

    /**
     * @brief 빌드 설정이 정하는 기본 로그 레벨을 반환한다.
     *
     * Debug는 Debug 이상을 남기고, Release는 Error 이상만 남긴다. 호출자가
     * setLevel()을 호출하지 않았을 때 이 값이 쓰인다.
     *
     * NDEBUG 판정은 assert.hpp와 같은 기준을 쓴다. 두 기준이 갈라지면
     * "이 로그는 왜 안 보이나"를 설명하기 어려워진다.
     */
    [[nodiscard]]
    static LogLevel defaultLevel() noexcept;

    /**
     * @brief 현재 전역 최소 로그 레벨을 반환한다.
     *
     * @return 전역 최소 로그 레벨.
     */
    [[nodiscard]]
    LogLevel level() const noexcept;

    /**
     * @brief 특정 카테고리의 로그 레벨을 설정한다.
     *
     * 설정된 레벨은 전역 레벨을 덮어쓴다.
     *
     * @param category 로그 카테고리.
     * @param level 해당 카테고리에 적용할 최소 로그 레벨.
     */
    void setCategoryLevel(LogCategory category, LogLevel level) noexcept;

    /**
     * @brief 특정 카테고리의 로그 레벨 설정을 제거한다.
     *
     * 제거된 카테고리는 전역 로그 레벨을 사용한다.
     *
     * @param category 로그 카테고리.
     */
    void resetCategoryLevel(LogCategory category) noexcept;

    /**
     * @brief 특정 카테고리에 적용되는 유효 로그 레벨을 반환한다.
     *
     * 카테고리에 별도의 레벨이 설정되어 있으면 해당 레벨을 반환하고,
     * 그렇지 않으면 전역 로그 레벨을 반환한다.
     *
     * @param category 로그 카테고리.
     *
     * @return 해당 카테고리에 적용되는 유효 로그 레벨.
     */
    [[nodiscard]]
    LogLevel categoryLevel(LogCategory category) const noexcept;

private:
    /**
     * @brief 주어진 로그가 현재 설정에 따라 기록되어야 하는지 확인한다.
     *
     * @param level 로그의 심각도.
     * @param category 로그 카테고리.
     *
     * @return 기록해야 하면 true를 반환한다.
     */
    [[nodiscard]]
    bool shouldLog(LogLevel level, LogCategory category) const noexcept;

private:
    using CategoryLevels =
        std::array<std::optional<LogLevel>, static_cast<std::size_t>(LogSubsystem::LogSubsystemCount)>;

    // Sink 하나로 생성된 경우를 위한 자리. spans_가 이 배열을 가리킨다.
    LogSink* singleSinkStorage_[1]{};

    // Sink는 소유하지 않는다. 호출자가 배열과 각 Sink의 수명을 유지해야 한다.
    std::span<LogSink*> sinks_;
    LogLevel level_{defaultLevel()};
    CategoryLevels categoryLevels_{};
    mutable std::mutex mutex_;
};

/**
 * @brief 전역 로깅 관리자를 등록한다.
 *
 * 전역 로깅 함수를 사용하기 전에 호출해야 한다.
 * 관리자의 소유권은 호출자에게 있으며, 이후의 모든 로깅 호출이 끝날 때까지 관리자가 살아 있어야 한다.
 * 초기화가 완료되고 다른 스레드가 로깅을 시작한 이후에는 관리자를 교체하지 않는 것을 전제로 한다.
 *
 * @param manager 등록할 로그 관리자.
 */
void setLogManager(LogManager& manager) noexcept;

/**
 * @brief 현재 등록된 전역 로깅 관리자를 반환한다.
 *
 * 등록된 관리자가 없으면 nullptr을 반환한다.
 *
 * @return 현재 전역 로그 관리자 또는 nullptr.
 */
[[nodiscard]]
LogManager* logManager() noexcept;

/**
 * @brief 로그 레벨의 문자열 표현을 반환한다.
 *
 * @param level 변환할 로그 레벨.
 *
 * @return 로그 레벨의 문자열 표현.
 */
[[nodiscard]]
std::string_view toString(LogLevel level) noexcept;

/**
 * @brief 로그 계층의 문자열 표현을 반환한다.
 *
 * @param layer 변환할 로그 계층.
 *
 * @return 로그 계층의 문자열 표현.
 */
[[nodiscard]]
std::string_view toString(LogLayer layer) noexcept;

/**
 * @brief 로그 하위 시스템의 문자열 표현을 반환한다.
 *
 * @param subsystem 변환할 로그 하위 시스템.
 *
 * @return 로그 하위 시스템의 문자열 표현. LogSubsystem::None이면 빈 문자열.
 */
[[nodiscard]]
std::string_view toString(LogSubsystem subsystem) noexcept;

/**
 * @brief 로그 줄에 이어 붙일 카테고리 그룹을 만든다.
 *
 * 두 시크가 같은 형식을 쓰므로 한 곳에 둔다. 계층 그룹은 항상 만들고 하위
 * 시스템 그룹은 있을 때만 덧붙인다.
 *
 * @param[out] out 이어 붙일 대상 문자열.
 * @param category 로그 카테고리.
 *
 * @note 예: Platform/Win32는 "[Platform][Win32]", Engine/None은 "[Engine]".
 */
void appendCategoryGroups(std::string& out, LogCategory category);

/**
 * @brief 로그를 기록한다.
 *
 * 전역 로그 관리자가 등록되지 않은 경우 로그를 버린다.
 *
 * @param level 로그의 심각도.
 * @param category 로그가 발생한 계층과 하위 시스템.
 * @param message 로그 메시지.
 * @param location 로그가 호출된 소스 위치.
 */
void log(LogLevel level, LogCategory category, std::string message,
         std::source_location location = std::source_location::current()) noexcept;

/**
 * @brief 형식 문자열과 인자로 로그 메시지를 만들어 반환한다.
 *
 * std::format을 사용한다. 형식 지정자가 잘못되면 컴파일 시점에
 * 오류가 발생하므로 런타임에 실패하지 않는다.
 *
 * 인자가 없는 경우에도 호출할 수 있어, 매크로가 항상 이 경로를 거치게 한다.
 *
 * @param fmt 형식 문자열.
 * @param args 형식 문자열에 대응하는 인자.
 *
 * @return 포맷이 적용된 로그 메시지.
 */
template <typename... Args>
[[nodiscard]]
std::string formatMessage(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

} // namespace nxt::core::log

// -----------------------------------------------------------------------------
// 전역 로깅 프론트엔드
// -----------------------------------------------------------------------------

/**
 * @brief 로그 카테고리를 만들어 매크로에 넘긴다.
 *
 * 호출부는 layer와 subsystem 이름을 두 인자로만 준다. 구조체 조립은 여기서
 * 끝나므로 호출부가 중괄호 두 겹을 손으로 쓰지 않는다.
 */
#define NXT_LOG_CATEGORY(layer, subsystem)                                                                             \
    ::nxt::core::log::LogCategory {                                                                                    \
        ::nxt::core::log::LogLayer::layer, ::nxt::core::log::LogSubsystem::subsystem                                   \
    }

#define NXT_LOG_TRACE(layer, subsystem, ...)                                                                           \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Trace, NXT_LOG_CATEGORY(layer, subsystem),                       \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())

#define NXT_LOG_DEBUG(layer, subsystem, ...)                                                                           \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Debug, NXT_LOG_CATEGORY(layer, subsystem),                       \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())

#define NXT_LOG_INFO(layer, subsystem, ...)                                                                            \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Info, NXT_LOG_CATEGORY(layer, subsystem),                        \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())

#define NXT_LOG_WARN(layer, subsystem, ...)                                                                            \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Warn, NXT_LOG_CATEGORY(layer, subsystem),                        \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())

#define NXT_LOG_ERROR(layer, subsystem, ...)                                                                           \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Error, NXT_LOG_CATEGORY(layer, subsystem),                       \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())

#define NXT_LOG_FATAL(layer, subsystem, ...)                                                                           \
    ::nxt::core::log::log(::nxt::core::log::LogLevel::Fatal, NXT_LOG_CATEGORY(layer, subsystem),                       \
                          ::nxt::core::log::formatMessage(__VA_ARGS__), std::source_location::current())
