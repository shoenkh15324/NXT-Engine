#include <atomic>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/core/jobs/job.hpp>
#include <nxt/core/jobs/job_counter.hpp>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using nxt::core::jobs::Job;
using nxt::core::jobs::JobCounter;

} // namespace

TEST_CASE("JobCounter는 복사하거나 이동할 수 없다.") {
    static_assert(!std::is_copy_constructible_v<JobCounter>);
    static_assert(!std::is_copy_assignable_v<JobCounter>);
    static_assert(!std::is_move_constructible_v<JobCounter>);
    static_assert(!std::is_move_assignable_v<JobCounter>);
}

TEST_CASE("JobCounter는 제출 수만큼 완료되면 wait에서 반환한다.") {
    constexpr std::uint32_t kThreadCount = 8;
    constexpr std::uint32_t kJobsPerThread = 8;
    constexpr std::uint32_t kJobCount = kThreadCount * kJobsPerThread;

    JobCounter counter;
    std::atomic<std::uint32_t> completed{0};

    counter.add(kJobCount);
    REQUIRE(counter.pending() == kJobCount);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (std::uint32_t i = 0; i < kThreadCount; ++i) {
        workers.emplace_back([&counter, &completed] {
            for (std::uint32_t job = 0; job < kJobsPerThread; ++job) {
                // 결과를 먼저 남기고 완료를 알린다. wait()가 반환하면 이 값이 보인다.
                completed.fetch_add(1, std::memory_order_relaxed);
                counter.complete();
            }
        });
    }

    counter.wait();

    CHECK(counter.pending() == 0);
    CHECK(completed.load(std::memory_order_relaxed) == kJobCount);

    for (std::thread& worker : workers) {
        worker.join();
    }
}

TEST_CASE("JobCounter는 완료가 없으면 wait가 즉시 반환한다.") {
    JobCounter counter;

    counter.wait();

    CHECK(counter.pending() == 0);
}

TEST_CASE("JobCounter는 0이 된 뒤 다시 쓸 수 있다.") {
    JobCounter counter;

    for (int round = 0; round < 3; ++round) {
        counter.add(3);
        counter.complete();
        counter.complete();
        CHECK(counter.pending() == 1);

        counter.complete();
        counter.wait();
        CHECK(counter.pending() == 0);
    }
}

TEST_CASE("JobCounter는 0을 더해도 상태가 바뀌지 않는다.") {
    JobCounter counter;

    counter.add(0);

    CHECK(counter.pending() == 0);
    counter.wait();
}

TEST_CASE("Job은 실행 함수와 카운터가 모두 있어야 유효하다.") {
    JobCounter counter;

    const Job empty{};
    const Job noCounter{[] {}, nullptr};
    const Job noFunction{{}, &counter};
    const Job complete{[] {}, &counter};

    CHECK_FALSE(empty.valid());
    CHECK_FALSE(noCounter.valid());
    CHECK_FALSE(noFunction.valid());
    CHECK(complete.valid());
}
