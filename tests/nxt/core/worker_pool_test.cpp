#include "fake_thread_source.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/core/jobs/job.hpp>
#include <nxt/core/jobs/job_counter.hpp>
#include <nxt/core/jobs/std_thread_source.hpp>
#include <nxt/core/jobs/worker.hpp>
#include <nxt/core/jobs/worker_pool.hpp>
#include <thread>
#include <utility>

namespace {

using nxt::core::jobs::defaultWorkerCount;
using nxt::core::jobs::Job;
using nxt::core::jobs::JobCounter;
using nxt::core::jobs::normalizeWorkerCount;
using nxt::core::jobs::StdThreadSource;
using nxt::core::jobs::WorkerPool;
using nxt::test::FakeThreadSource;

} // namespace

TEST_CASE("worker 수는 0코어에서 1로 보정된다.") {
    static_assert(normalizeWorkerCount(0) == 1);
    static_assert(normalizeWorkerCount(1) == 1);

    CHECK(normalizeWorkerCount(0) == 1);
    CHECK(normalizeWorkerCount(8) == 8);

    CHECK(defaultWorkerCount() >= 1);

    // 보정된 수를 그대로 pool에 넘기면 worker가 1개 만들어진다.
    const WorkerPool pool(normalizeWorkerCount(0), 4);
    CHECK(pool.workerCount() == 1);
    CHECK_FALSE(pool.running());
}

TEST_CASE("WorkerPool은 fake source로 worker 수만큼 만들고 모두 join한다.") {
    FakeThreadSource source;
    WorkerPool pool(3, 4);
    JobCounter counter;
    int runs[3] = {0, 0, 0};

    pool.start(source);
    CHECK(pool.running());
    CHECK(pool.workerCount() == 3);
    REQUIRE(source.startCount() == 3);

    counter.add(3);
    for (std::size_t i = 0; i < pool.workerCount(); ++i) {
        CHECK(pool.worker(i).trySubmit(Job{[&runs, i] { ++runs[i]; }, &counter}));
    }

    // fake source는 스레드를 만들지 않는다. stop() 뒤에 worker 루프를 직접 돌린다.
    pool.stop();
    for (std::size_t i = 0; i < pool.workerCount(); ++i) {
        source.body(i)();
    }

    CHECK(runs[0] == 1);
    CHECK(runs[1] == 1);
    CHECK(runs[2] == 1);
    CHECK(counter.pending() == 0);

    pool.join();
    CHECK(source.joinCount() == 3);
    CHECK_FALSE(pool.running());
}

TEST_CASE("WorkerPool은 실제 스레드 N개에 잡을 나눠 모두 실행한다.") {
    constexpr std::uint32_t kJobCount = 1000;

    StdThreadSource source;
    WorkerPool pool(2, 32);
    JobCounter counter;
    std::atomic<std::uint32_t> runs{0};

    pool.start(source);
    REQUIRE(pool.running());

    // 큐 push보다 add가 먼저다.
    counter.add(kJobCount);
    for (std::uint32_t i = 0; i < kJobCount; ++i) {
        Job job{[&runs] { runs.fetch_add(1, std::memory_order_relaxed); }, &counter};
        nxt::core::jobs::Worker& target = pool.worker(i % pool.workerCount());

        // 가득 차면 잡은 move되지 않으므로 그대로 다시 넣는다.
        while (!target.trySubmit(std::move(job))) {
            std::this_thread::yield();
        }
    }

    counter.wait();
    CHECK(runs.load(std::memory_order_relaxed) == kJobCount);

    pool.stop();
    pool.join();
    CHECK_FALSE(pool.running());
}
