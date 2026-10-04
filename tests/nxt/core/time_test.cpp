#include <chrono>
#include <doctest/doctest.h>
#include <nxt/core/time/time.hpp>
#include <thread>

namespace {

using nxt::time::Duration;
using nxt::time::kNsPerMs;
using nxt::time::kNsPerUs;
using nxt::time::now;
using nxt::time::Timestamp;

} // namespace

// 단위 상수는 nanosecond 기준으로 고정된다.
TEST_CASE("단위 상수는 nanosecond 기준이다") {
    CHECK(kNsPerUs == 1000u);
    CHECK(kNsPerMs == 1000000u);
}

// Timestamp에 임의 값을 만들 수 없으므로 자기 자신을 빼는 경로로 0을 확인한다.
TEST_CASE("같은 시점의 차이는 0이다") {
    const Timestamp mark = now();
    CHECK((mark - mark).ns() == 0);
}

// Duration의 기본값은 0초다.
TEST_CASE("Duration 기본값은 0이다") {
    CHECK(Duration{}.ns() == 0u);
}

TEST_CASE("now는 역행하지 않는다") {
    Timestamp previous = now();

    for (int i = 0; i < 1000; ++i) {
        const Timestamp current = now();
        CHECK(current >= previous);
        previous = current;
    }
}

TEST_CASE("비교 연산은 defaulted spaceship에서 파생된다") {
    const Timestamp mark = now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const Timestamp later = now();

    // std::strong_ordering 을 0과 직접 비교하면 MSVC의 _Literal_zero(consteval) 생성자가
    // 상수식 안에서 호출 불가라 컴파일이 깨진다. defaulted spaceship이 파생한 비교만 쓴다.
    CHECK(mark < later);
    CHECK(later > mark);
    CHECK(mark == mark);
    CHECK_FALSE(mark != mark);
}

// steady_clock은 지정 시간보다 일찍 지나가지 않으므로 하한으로 쓸 수 있다.
// 상한은 OS 스케줄러 지연 때문에 검증할 수 없다.
TEST_CASE("경과 시간은 sleep한 시간을 넘는다") {
    const Timestamp mark = now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    CHECK((now() - mark).ns() >= 2u * kNsPerMs);
}

// 실제 시간 대신 이미 얻은 Duration을 더하므로 결과가 정확하다.
TEST_CASE("Duration 누적을 반복하면 합이 된다") {
    const Timestamp mark = now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const Timestamp later = now();
    const Duration slice = later - mark;

    Duration total{};
    total += slice;
    total += slice;
    total += slice;

    CHECK(total.ns() == 3u * slice.ns());
}
