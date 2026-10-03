#include <doctest/doctest.h>
#include <nxt/core/diagnostics/assert.hpp>
#include <string>

namespace {

nxt::diag::AssertInfo gReceived{};
int gCallCount = 0;

void recordingHandler(const nxt::diag::AssertInfo& info) noexcept {
    gReceived = info;
    ++gCallCount;
}

} // namespace

TEST_CASE("notify는 등록된 핸들러에 정보를 그대로 전달한다") {
    nxt::diag::setAssertHandler(&recordingHandler);

    nxt::diag::notifyAssertFailure({"a == b", "a와 b가 달라야 한다", "assert_test.cpp", "test_case", 17});

    CHECK(gCallCount == 1);
    CHECK(std::string(gReceived.expression) == "a == b");
    CHECK(std::string(gReceived.message) == "a와 b가 달라야 한다");
    CHECK(std::string(gReceived.file) == "assert_test.cpp");
    CHECK(std::string(gReceived.function) == "test_case");
    CHECK(gReceived.line == 17);
}

TEST_CASE("조건을 만족하면 핸들러가 호출되지 않는다") {
    gCallCount = 0;
    nxt::diag::setAssertHandler(&recordingHandler);

    const int value = 1;
    NXT_ASSERT(value == 1);
    NXT_ASSERT_MSG(value > 0, "value는 양수여야 한다");
    NXT_VERIFY(value < 2);
    NXT_VERIFY_MSG(value >= 1, "value는 1 이상이어야 한다");

    CHECK(gCallCount == 0);

    // Release에서 NXT_ASSERT에만 쓰이는 value가 미사용 경고가 되지 않게 한다.
    CHECK(value == 1);
}

TEST_CASE("NXT_ASSERT는 Release에서 조건을 평가하지 않고 NXT_VERIFY는 평가한다") {
    int assertEvaluated = 0;
    int verifyEvaluated = 0;

    const auto assertCondition = [&assertEvaluated] {
        ++assertEvaluated;
        return true;
    };
    const auto verifyCondition = [&verifyEvaluated] {
        ++verifyEvaluated;
        return true;
    };

    NXT_ASSERT(assertCondition());
    NXT_VERIFY(verifyCondition());

#if defined(NDEBUG)
    CHECK(assertEvaluated == 0);
#else
    CHECK(assertEvaluated == 1);
#endif
    CHECK(verifyEvaluated == 1);
}
