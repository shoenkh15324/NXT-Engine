#include "fake_thread_source.hpp"

#include <atomic>
#include <doctest/doctest.h>
#include <nxt/core/jobs/job.hpp>
#include <nxt/core/jobs/job_counter.hpp>
#include <nxt/core/jobs/std_thread_source.hpp>
#include <nxt/core/jobs/worker.hpp>
#include <thread>
#include <utility>

namespace {

using nxt::core::jobs::Job;
using nxt::core::jobs::JobCounter;
using nxt::core::jobs::StdThreadSource;
using nxt::core::jobs::Worker;
using nxt::test::FakeThreadSource;

} // namespace

TEST_CASE("Worker는 fake source에서 큐에 남은 잡을 모두 실행하고 종료한다.") {
    FakeThreadSource source;
    Worker worker(4);
    JobCounter counter;
    int runs = 0;

    worker.start(source);
    REQUIRE(source.startCount() == 1);
    CHECK(worker.running());

    counter.add(2);
    CHECK(worker.trySubmit(Job{[&runs] { ++runs; }, &counter}));
    CHECK(worker.trySubmit(Job{[&runs] { runs += 10; }, &counter}));

    // fake source는 스레드를 만들지 않는다. body를 직접 실행해 루프를 돌린다.
    // stop()이 먼저여도 큐에 있는 잡을 모두 실행한 뒤 루프가 끝난다.
    worker.stop();
    source.body(0)();

    CHECK(runs == 11);
    CHECK(counter.pending() == 0);

    worker.join();
    CHECK(source.joinCount() == 1);
    CHECK_FALSE(worker.running());
}

TEST_CASE("Worker는 큐가 가득 차면 제출을 거부하고 인자를 move하지 않는다.") {
    FakeThreadSource source;
    Worker worker(1);
    JobCounter counter;
    int runs = 0;

    worker.start(source);

    counter.add(1);
    CHECK(worker.trySubmit(Job{[&runs] { ++runs; }, &counter}));

    Job rejected{[&runs] { runs += 100; }, &counter};
    CHECK_FALSE(worker.trySubmit(std::move(rejected)));
    CHECK(rejected.valid()); // 실패한 push는 인자를 move하지 않는다.

    worker.stop();
    source.body(0)();

    CHECK(runs == 1);
    CHECK(counter.pending() == 0);

    worker.join();
}

TEST_CASE("Worker는 실제 스레드에서 제출한 잡 N개를 모두 실행한다.") {
    constexpr std::uint32_t kJobCount = 2000;

    StdThreadSource source;
    Worker worker(64);
    JobCounter counter;
    std::atomic<std::uint32_t> runs{0};
    std::atomic<std::thread::id> workerId{};

    worker.start(source);

    // 큐 push보다 add가 먼저다. 1-B2의 제출 순서 계약 그대로다.
    counter.add(kJobCount);
    for (std::uint32_t i = 0; i < kJobCount; ++i) {
        Job job{
            [&runs, &workerId] {
                workerId.store(std::this_thread::get_id());
                runs.fetch_add(1, std::memory_order_relaxed);
            },
            &counter,
        };

        // 가득 차면 잡은 move되지 않으므로 그대로 다시 넣는다.
        while (!worker.trySubmit(std::move(job))) {
            std::this_thread::yield();
        }
    }

    counter.wait();

    CHECK(runs.load(std::memory_order_relaxed) == kJobCount);
    CHECK(workerId.load() != std::this_thread::get_id());

    worker.stop();
    worker.join();
    CHECK_FALSE(worker.running());
}
