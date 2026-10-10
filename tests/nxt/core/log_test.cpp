#include <doctest/doctest.h>
#include <nxt/core/diagnostics/log.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using nxt::core::log::LogCategory;
using nxt::core::log::LogLayer;
using nxt::core::log::LogLevel;
using nxt::core::log::LogManager;
using nxt::core::log::LogRecord;
using nxt::core::log::LogSubsystem;

/**
 * 전달 여부와 레코드 내용을 함께 확인하는 sink다.
 * LogManager가 무엇을 전달하는지 관찰하는 용도이며 콘솔이나 파일로 쓰지 않는다.
 */
class RecordingSink : public nxt::core::log::LogSink {
public:
    void write(const LogRecord& record) noexcept override {
        records_.push_back(record);
    }

    [[nodiscard]]
    int count() const noexcept {
        return static_cast<int>(records_.size());
    }

    [[nodiscard]]
    const LogRecord& at(const int index) const noexcept {
        return records_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]]
    const LogRecord& last() const noexcept {
        return records_.back();
    }

    void clear() noexcept {
        records_.clear();
    }

private:
    std::vector<LogRecord> records_;
};

/**
 * 전역 테스트 하네스는 정적 수명을 갖는다.
 *
 * setLogManager에는 해제 API가 없어 한 번 등록하면 이후 테스트까지 살아 있어야 한다.
 * 테스트 지역 변수를 등록하면 그 테스트가 끝난 뒤 남은 테스트들이 죽은
 * manager를 역참조해 segfault가 난다.
 */
RecordingSink gSink;
nxt::core::log::LogManager gManager(gSink);

/**
 * 테스트 사이에 새는 전역 상태를 초기 상태로 되돌린다.
 * 카테고리 override도 함께 지워야 다른 테스트에 영향을 주지 않는다.
 */
void resetGlobal() {
    gSink.clear();
    // 여기서는 기본값이 아니라 알려진 레벨이 필요하다. defaultLevel()을 쓰면
    // Release에서 Error가 되어 Info를 남기는 테스트가 전부 걸린다.
    // 기본값 자체는 별도 테스트가 생성자로 직접 확인한다.
    gManager.setLevel(LogLevel::Info);
    // 카테고리를 나열하면 하위 시스템이 늘 때마다 여기서 빠진다.
    // override 배열은 LogSubsystem 값으로 색인하므로 값 범위를 돈다.
    for (std::size_t i = 0; i < static_cast<std::size_t>(LogSubsystem::LogSubsystemCount); ++i) {
        gManager.resetCategoryLevel(LogCategory{LogLayer::Core, static_cast<LogSubsystem>(i)});
    }
    nxt::core::log::setLogManager(gManager);
}

/**
 * doctest는 비교식을 두 피연산자로 분해한다. enum과 string_view를 그대로 두면
 * 실패 메시지를 만들려고 operator+를 찾으려다 컴파일이 깨지므로
 * 비교 대상은 전부 std::string으로 변환한다.
 */
[[nodiscard]]
std::string text(const std::string_view value) {
    return std::string(value);
}

[[nodiscard]]
std::string levelName(const LogLevel level) {
    return text(nxt::core::log::toString(level));
}

/**
 * @brief 카테고리가 로그 줄에 표시되는 형태를 그대로 만들어 비교한다.
 *
 * 레코드에서 꺼낸 카테고리와 기대값을 같은 함수로 변환해야 비교가 의미가
 * 있으므로, toString이 아니라 appendCategoryGroups를 쓴다.
 */
[[nodiscard]]
std::string categoryName(const LogCategory category) {
    std::string text;
    nxt::core::log::appendCategoryGroups(text, category);
    return text;
}

} // namespace

TEST_CASE("기본 로그 레벨은 빌드 설정을 따른다") {
    // resetGlobal()은 레벨을 강제로 설정하므로 이 테스트에서 쓸 수 없다.
    // 전역 gManager를 그대로 쓰고, 별도 인스턴스가 가진 기본값을 확인한다.
    CHECK(levelName(LogManager::defaultLevel()) == levelName(LogManager{gSink}.level()));
#if defined(NDEBUG)
    CHECK(levelName(LogManager::defaultLevel()) == levelName(LogLevel::Error));
#else
    CHECK(levelName(LogManager::defaultLevel()) == levelName(LogLevel::Debug));
#endif
}

TEST_CASE("로그 레벨을 직접 지정하면 기본값을 덮어쓴다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Error);

    CHECK(levelName(gManager.level()) == levelName(LogLevel::Error));
}

TEST_CASE("전역 레벨보다 낮은 로그는 sink에 전달되지 않는다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Error);

    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "filtered out");
    CHECK(gSink.count() == 0);

    nxt::core::log::log(LogLevel::Error, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "passed through");
    CHECK(gSink.count() == 1);
    CHECK(levelName(gSink.last().level) == levelName(LogLevel::Error));
}

TEST_CASE("레벨 경계값은 유효 레벨과 같을 때 통과한다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Warn);

    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "below boundary");
    CHECK(gSink.count() == 0);

    // 결정사항 §3의 규칙은 level >= effectiveLevel이므로 경계값은 통과한다.
    nxt::core::log::log(LogLevel::Warn, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "on boundary");
    CHECK(gSink.count() == 1);
}

TEST_CASE("카테고리 override가 없을 때 전역 레벨을 사용한다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Warn);

    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Renderer, LogSubsystem::World}, "filtered out");
    CHECK(gSink.count() == 0);

    nxt::core::log::log(LogLevel::Warn, LogCategory{LogLayer::Renderer, LogSubsystem::World}, "passed through");
    CHECK(gSink.count() == 1);
}

TEST_CASE("카테고리 override는 그 카테고리에만 적용된다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Error);
    gManager.setCategoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World}, LogLevel::Trace);

    // 전역은 Error지만 Renderer는 Trace로 덮였으므로 Info도 통과한다.
    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Renderer, LogSubsystem::World}, "renderer override");
    CHECK(gSink.count() == 1);
    CHECK(categoryName(gSink.last().category) == categoryName(LogCategory{LogLayer::Renderer, LogSubsystem::World}));

    // 다른 카테고리는 전역 레벨을 그대로 따른다.
    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "core follows global");
    CHECK(gSink.count() == 1);
}

TEST_CASE("카테고리마다 다른 override를 줄 수 있다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Fatal);
    gManager.setCategoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World}, LogLevel::Trace);
    gManager.setCategoryLevel(LogCategory{LogLayer::Core, LogSubsystem::Memory}, LogLevel::Warn);

    nxt::core::log::log(LogLevel::Debug, LogCategory{LogLayer::Renderer, LogSubsystem::World}, "renderer");
    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "core filtered");
    nxt::core::log::log(LogLevel::Warn, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "core boundary");

    CHECK(gSink.count() == 2);
}

TEST_CASE("resetCategoryLevel은 전역 레벨로 되돌린다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Error);
    gManager.setCategoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World}, LogLevel::Trace);

    gManager.resetCategoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World});

    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Renderer, LogSubsystem::World}, "back to global");
    CHECK(gSink.count() == 0);
}

TEST_CASE("categoryLevel은 override가 없으면 전역 레벨을 반환한다") {
    resetGlobal();
    gManager.setLevel(LogLevel::Warn);

    CHECK(levelName(gManager.categoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World})) ==
          levelName(LogLevel::Warn));

    gManager.setCategoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World}, LogLevel::Error);
    CHECK(levelName(gManager.categoryLevel(LogCategory{LogLayer::Renderer, LogSubsystem::World})) ==
          levelName(LogLevel::Error));
}

TEST_CASE("logManager는 등록된 manager를 반환한다") {
    resetGlobal();

    CHECK(nxt::core::log::logManager() == &gManager);
}

TEST_CASE("LogRecord는 메시지와 발신 정보를 모두 담는다") {
    resetGlobal();

    nxt::core::log::log(LogLevel::Info, LogCategory{LogLayer::Assets, LogSubsystem::Loaders}, "record fields");

    REQUIRE(gSink.count() == 1);

    const LogRecord& record = gSink.last();
    CHECK(levelName(record.level) == levelName(LogLevel::Info));
    CHECK(categoryName(record.category) == categoryName(LogCategory{LogLayer::Assets, LogSubsystem::Loaders}));
    CHECK(record.message == text("record fields"));
    CHECK(record.threadId == std::this_thread::get_id());
    CHECK(record.timestamp.time_since_epoch().count() > 0);
}

TEST_CASE("매크로는 실제 호출 위치를 캡처한다") {
    resetGlobal();

    const int expectedLine = __LINE__ + 1;
    NXT_LOG_INFO(Core, Memory, "macro captures location");

    REQUIRE(gSink.count() == 1);
    CHECK(gSink.last().location.line() == expectedLine);
    CHECK(gSink.last().location.column() > 0);
    CHECK(levelName(gSink.last().level) == levelName(LogLevel::Info));
}

TEST_CASE("매크로는 카테고리를 인자로 받는다") {
    resetGlobal();

    NXT_LOG_WARN(Graphics, Rhi, "macro category");

    REQUIRE(gSink.count() == 1);
    CHECK(categoryName(gSink.last().category) == categoryName(LogCategory{LogLayer::Graphics, LogSubsystem::Rhi}));
    CHECK(levelName(gSink.last().level) == levelName(LogLevel::Warn));
}

TEST_CASE("매크로는 인자가 없는 메시지를 받는다") {
    resetGlobal();

    NXT_LOG_INFO(Core, Memory, "no arguments");

    REQUIRE(gSink.count() == 1);
    CHECK(gSink.last().message == text("no arguments"));
}

TEST_CASE("매크로는 format 인자를 적용한다") {
    resetGlobal();

    NXT_LOG_ERROR(Assets, Loaders, "code = {}", 42);
    NXT_LOG_ERROR(Assets, Loaders, "{} of {} failed", 3, 10);

    REQUIRE(gSink.count() == 2);
    CHECK(gSink.at(0).message == text("code = 42"));
    CHECK(gSink.at(1).message == text("3 of 10 failed"));
}

TEST_CASE("포맷 지정자 개수만큼 인자를 넘긴다") {
    resetGlobal();

    NXT_LOG_INFO(Core, Memory, "{} {} {}", 1, 2, 3);
    NXT_LOG_INFO(Core, Memory, "{} of {}", 1, 2);

    REQUIRE(gSink.count() == 2);
    CHECK(gSink.at(0).message == text("1 2 3"));
    CHECK(gSink.at(1).message == text("1 of 2"));
}

// 포맷 인자가 맞지 않으면 컴파일 시점에 실패하는 것이 의도된 동작이다.
// std::format_string이 형식 지정자와 인자 개수를 맞춰 검증하므로
// 잘못된 호출을 런타임 테스트로 만들 수 없다. 다음 코드는 컴파일되지 않는다.
//
//     NXT_LOG_INFO(Core, Memory, "{} {} {}", 1);   // 인자가 하나 부족
//     NXT_LOG_INFO(Core, Memory, "literal");       // 지정자가 있는데 인자가 없음
TEST_CASE("포맷이 필요 없는 메시지도 그대로 기록된다") {
    resetGlobal();

    NXT_LOG_INFO(Core, Memory, "literal only");

    REQUIRE(gSink.count() == 1);
    CHECK(gSink.last().message == text("literal only"));
}

TEST_CASE("formatMessage는 문자열만 받아도 동작한다") {
    const std::string message = nxt::core::log::formatMessage("plain message");

    CHECK(message == text("plain message"));
}

TEST_CASE("포맷 인자가 비어 있어도 남는 문자열을 유지한다") {
    resetGlobal();

    NXT_LOG_INFO(Core, Memory, "prefix {} suffix", 1);

    REQUIRE(gSink.count() == 1);
    CHECK(gSink.last().message == text("prefix 1 suffix"));
}

TEST_CASE("레벨을 문자열로 변환한다") {
    CHECK(levelName(LogLevel::Trace) == text("TRACE"));
    CHECK(levelName(LogLevel::Debug) == text("DEBUG"));
    CHECK(levelName(LogLevel::Info) == text("INFO"));
    CHECK(levelName(LogLevel::Warn) == text("WARN"));
    CHECK(levelName(LogLevel::Error) == text("ERROR"));
    CHECK(levelName(LogLevel::Fatal) == text("FATAL"));
}

TEST_CASE("카테고리를 대괄호 그룹으로 표시한다") {
    CHECK(categoryName(LogCategory{LogLayer::Core, LogSubsystem::Memory}) == text("[Core][Memory]"));
    CHECK(categoryName(LogCategory{LogLayer::Platform, LogSubsystem::Win32}) == text("[Platform][Win32]"));
    CHECK(categoryName(LogCategory{LogLayer::Renderer, LogSubsystem::World}) == text("[Renderer][World]"));
    CHECK(categoryName(LogCategory{LogLayer::Graphics, LogSubsystem::Rhi}) == text("[Graphics][Rhi]"));
    CHECK(categoryName(LogCategory{LogLayer::Assets, LogSubsystem::Loaders}) == text("[Assets][Loaders]"));
    CHECK(categoryName(LogCategory{LogLayer::Engine, LogSubsystem::FrameLoop}) == text("[Engine][FrameLoop]"));
}

TEST_CASE("하위 시스템이 없으면 계층 그룹 하나만 만든다") {
    CHECK(categoryName(LogCategory{LogLayer::Engine, LogSubsystem::None}) == text("[Engine]"));
    CHECK(categoryName(LogCategory{LogLayer::Core, LogSubsystem::None}) == text("[Core]"));
}

TEST_CASE("모든 하위 시스템에 대응 문자열이 있다") {
    // 새 하위 시스템을 추가하면서 문자열을 빠뜨리는 것을 잡는다.
    for (std::size_t i = 0; i < static_cast<std::size_t>(LogSubsystem::LogSubsystemCount); ++i) {
        const auto subsystem = static_cast<LogSubsystem>(i);
        const std::string_view name = nxt::core::log::toString(subsystem);

        if (subsystem == LogSubsystem::None) {
            CHECK(name.empty());
        } else {
            CHECK(name != "Unknown");
            CHECK(!name.empty());
        }
    }
}

TEST_CASE("모든 계층에 대응 문자열이 있다") {
    for (const LogLayer layer : {LogLayer::Core, LogLayer::Platform, LogLayer::Graphics, LogLayer::Renderer,
                                 LogLayer::Assets, LogLayer::Engine}) {
        CHECK(nxt::core::log::toString(layer) != "Unknown");
    }
}

TEST_CASE("Sink가 여러 개면 모두 같은 레코드를 받는다") {
    resetGlobal();

    RecordingSink first;
    RecordingSink second;

    nxt::core::log::LogSink* sinks[] = {&first, &second};
    nxt::core::log::LogManager manager(sinks);

    // 전역 manager가 아니라 지역 manager를 직접 쓴다.
    // 전역 로깅 함수는 gManager로 가므로 지역 manager를 관찰할 수 없다.
    // fan-out을 보는 테스트이므로 레벨은 명시한다. 기본값은 Release에서 Error라
    // Info 레코드가 걸러져 검증할 수 없게 된다.
    manager.setLevel(LogLevel::Info);
    manager.write(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "two sinks");

    REQUIRE(first.count() == 1);
    REQUIRE(second.count() == 1);

    // 두 Sink는 같은 레코드를 받아야 한다. 각자 포맷해선 안 된다.
    CHECK(first.last().message == second.last().message);
    CHECK(first.last().timestamp == second.last().timestamp);
    CHECK(first.last().threadId == second.last().threadId);
    CHECK(categoryName(first.last().category) == categoryName(second.last().category));
}

TEST_CASE("필터는 Sink 수와 무관하게 한 번만 동작한다") {
    resetGlobal();

    RecordingSink first;
    RecordingSink second;

    nxt::core::log::LogSink* sinks[] = {&first, &second};
    nxt::core::log::LogManager manager(sinks);
    manager.setLevel(LogLevel::Error);

    manager.write(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "filtered out");

    // 필터가 먼저 동작하므로 어느 Sink에도 전달되지 않는다.
    CHECK(first.count() == 0);
    CHECK(second.count() == 0);
}

TEST_CASE("Sink 목록이 비면 아무것도 기록되지 않는다") {
    resetGlobal();

    RecordingSink sink;
    nxt::core::log::LogSink* sinks[] = {nullptr};
    nxt::core::log::LogManager manager(sinks);

    // nullptr은 건너뛰므로 크래시 없이 아무것도 하지 않는다.
    manager.write(LogLevel::Fatal, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "null sink");
}

TEST_CASE("Sink 하나로 만든 manager와 여러 개로 만든 manager는 같은 경로를 쓴다") {
    resetGlobal();

    RecordingSink single;
    nxt::core::log::LogManager singleManager(single);

    RecordingSink first;
    RecordingSink second;
    nxt::core::log::LogSink* sinks[] = {&first, &second};
    nxt::core::log::LogManager multiManager(sinks);

    singleManager.setLevel(LogLevel::Info);
    multiManager.setLevel(LogLevel::Info);

    singleManager.write(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "same shape");
    multiManager.write(LogLevel::Info, LogCategory{LogLayer::Core, LogSubsystem::Memory}, "same shape");

    REQUIRE(single.count() == 1);
    REQUIRE(first.count() == 1);
    REQUIRE(second.count() == 1);

    // 포맷과 필터는 Sink 수와 무관하므로 전달되는 레코드가 동일하다.
    CHECK(single.last().message == first.last().message);
    CHECK(first.last().message == second.last().message);
}

TEST_CASE("LogManager는 복사할 수 없다") {
    // manager는 sink 참조를 들고 있어 복사하면 이중 해제가 된다.
    CHECK_FALSE(std::is_copy_constructible_v<nxt::core::log::LogManager>);
    CHECK_FALSE(std::is_copy_assignable_v<nxt::core::log::LogManager>);
}
