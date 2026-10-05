#include <chrono>
#include <doctest/doctest.h>
#include <nxt/core/time/time.hpp>
#include <nxt/core/time/timer.hpp>
#include <thread>

namespace {

using nxt::core::time::kNsPerMs;
using nxt::core::time::kNsPerUs;
using nxt::core::time::Timer;

// steady_clock은 지정 시간보다 일찍 지나가지 않으므로 하한으로 쓸 수 있다.
// 상한은 검증하지 않는다. OS가 슬립을 더 길게 만들 수 있기 때문이다.
void sleepMs(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace

TEST_CASE("생성 직후 측정이 시작된다") {
    Timer timer;

    REQUIRE(timer.running());

    sleepMs(2);

    CHECK(timer.elapsedNs() >= 2u * kNsPerMs);
}

TEST_CASE("reset은 경과를 0에서 다시 시작한다") {
    Timer timer;
    sleepMs(2);
    const auto before = timer.elapsedNs();
    REQUIRE(before >= 2u * kNsPerMs);

    timer.reset();

    CHECK(timer.running());
    CHECK(timer.elapsedNs() < before);
}

// 멈춘 상태는 시계를 읽지 않으므로 값이 정확히 고정된다.
TEST_CASE("stop한 뒤에는 경과가 증가하지 않는다") {
    Timer timer;
    sleepMs(2);
    timer.stop();
    const auto stopped = timer.elapsedNs();
    REQUIRE(stopped >= 2u * kNsPerMs);

    sleepMs(2);

    CHECK(timer.elapsedNs() == stopped);
    CHECK_FALSE(timer.running());
}

TEST_CASE("stop 이후 start는 누적 시간을 유지한 채 다시 잰다") {
    Timer timer;
    sleepMs(2);
    timer.stop();
    const auto first = timer.elapsedNs();

    sleepMs(20); // 멈춰 있는 동안은 누적되지 않는다
    timer.start();
    const auto resumed = timer.elapsedNs();

    CHECK(timer.running());
    CHECK(resumed >= first);
    CHECK(resumed < first + 5u * kNsPerMs);

    sleepMs(2);
    CHECK(timer.elapsedNs() >= resumed + 2u * kNsPerMs);
}

TEST_CASE("멈춘 상태에서 reset하면 경과가 0이고 멈춘 상태를 유지한다") {
    Timer timer;
    sleepMs(1);
    timer.stop();
    REQUIRE(timer.elapsedNs() > 0);

    timer.reset();

    CHECK(timer.elapsedNs() == 0);
    CHECK_FALSE(timer.running());

    sleepMs(1);
    CHECK(timer.elapsedNs() == 0);
}

TEST_CASE("stop은 멱등이다") {
    Timer timer;
    sleepMs(1);
    timer.stop();
    const auto stopped = timer.elapsedNs();

    timer.stop();

    CHECK(timer.elapsedNs() == stopped);
}

// 이미 실행 중일 때 start가 시작점을 다시 잡으면 경과가 누적값으로 되돌아간다.
TEST_CASE("start는 이미 실행 중이면 아무 일도 하지 않는다") {
    Timer timer;
    sleepMs(1);
    timer.stop();
    const auto accumulated = timer.elapsedNs();

    timer.start();
    sleepMs(1);
    const auto resumed = timer.elapsedNs();
    REQUIRE(resumed > accumulated);

    timer.start();

    CHECK(timer.elapsedNs() >= resumed);
    CHECK(timer.elapsedNs() > accumulated);
}

// 멈춘 상태에서는 시계를 안 읽으므로 세 단위가 같은 스냅샷에서 나온다.
TEST_CASE("멈춘 상태에서 단위 변환이 일관된다") {
    Timer timer;
    sleepMs(2);
    timer.stop();

    const auto nanoseconds = timer.elapsedNs();
    const auto microseconds = timer.elapsedUs();
    const auto milliseconds = timer.elapsedMs();

    CHECK(microseconds == nanoseconds / kNsPerUs);
    CHECK(milliseconds == nanoseconds / kNsPerMs);
    CHECK(milliseconds == microseconds / kNsPerUs);
}

// 실행 중에는 접근자마다 스냅샷이 다르므로 크기만 확인한다.
TEST_CASE("실행 중에는 단위 값이 시간에 따라 증가한다") {
    Timer timer;
    sleepMs(2);

    CHECK(timer.elapsedNs() >= 2u * kNsPerMs);
    CHECK(timer.elapsedUs() >= 2000u);
    CHECK(timer.elapsedMs() >= 2u);
}
