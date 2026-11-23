#include "fake_thread_source.hpp"

#include <atomic>
#include <doctest/doctest.h>
#include <memory>
#include <nxt/core/jobs/std_thread_source.hpp>
#include <nxt/core/jobs/thread_source.hpp>
#include <thread>

namespace {

using nxt::core::jobs::StdThreadSource;
using nxt::core::jobs::Thread;
using nxt::test::FakeThreadSource;

} // namespace

TEST_CASE("fake source는 스레드 없이 생성과 종료를 기록한다.") {
    FakeThreadSource source;
    std::atomic<int> runs{0};

    std::unique_ptr<Thread> first = source.start([&runs] { runs.fetch_add(1); });
    std::unique_ptr<Thread> second = source.start([&runs] { runs.fetch_add(1); });
    REQUIRE(first);
    REQUIRE(second);

    CHECK(source.startCount() == 2);
    CHECK(runs.load() == 0); // start()는 body를 실행하지 않는다.

    source.body(0)();
    CHECK(runs.load() == 1);

    first->join();
    CHECK(source.joinCount() == 1);

    second->join();
    CHECK(source.joinCount() == 2);
}

TEST_CASE("std source는 실제 스레드에서 body를 실행하고 join한다.") {
    StdThreadSource source;
    std::atomic<bool> ran{false};
    std::thread::id workerId{};

    std::unique_ptr<Thread> thread = source.start([&ran, &workerId] {
        workerId = std::this_thread::get_id();
        ran.store(true);
    });
    REQUIRE(thread);

    thread->join();

    // join 이후의 읽기는 안전하다. join이 스레드의 모든 쓰기를 앞선다.
    CHECK(ran.load());
    CHECK(workerId != std::this_thread::get_id());
}
