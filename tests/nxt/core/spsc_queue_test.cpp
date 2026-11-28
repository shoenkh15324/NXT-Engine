#include <cstddef>
#include <doctest/doctest.h>
#include <numeric>
#include <nxt/core/concurrency/spsc_queue.hpp>
#include <nxt/core/jobs/job.hpp>
#include <nxt/core/jobs/job_counter.hpp>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using nxt::core::concurrency::SpscQueue;
using nxt::core::jobs::Job;
using nxt::core::jobs::JobCounter;

/**
 * @brief move 여부를 관찰하는 테스트 전용 타입이다.
 *
 * 가득 찬 큐에 push가 실패했을 때 인자가 move되지 않는지 확인한다.
 */
class MoveTracker final {
public:
    MoveTracker() = default;
    explicit MoveTracker(int value) noexcept : value_(value) {}

    MoveTracker(const MoveTracker&) = delete;
    MoveTracker& operator=(const MoveTracker&) = delete;

    MoveTracker(MoveTracker&& other) noexcept : value_(other.value_) {
        other.movedFrom_ = true;
    }

    MoveTracker& operator=(MoveTracker&& other) noexcept {
        value_ = other.value_;
        movedFrom_ = false;
        other.movedFrom_ = true;
        return *this;
    }

    [[nodiscard]]
    int value() const noexcept {
        return value_;
    }

    [[nodiscard]]
    bool movedFrom() const noexcept {
        return movedFrom_;
    }

private:
    int value_{0};
    bool movedFrom_{false};
};

} // namespace

TEST_CASE("SpscQueue는 복사하거나 이동할 수 없다.") {
    static_assert(!std::is_copy_constructible_v<SpscQueue<int>>);
    static_assert(!std::is_copy_assignable_v<SpscQueue<int>>);
    static_assert(!std::is_move_constructible_v<SpscQueue<int>>);
    static_assert(!std::is_move_assignable_v<SpscQueue<int>>);
}

TEST_CASE("SpscQueue는 넣은 순서대로 꺼낸다.") {
    SpscQueue<int> queue(4);
    CHECK(queue.capacity() == 4);
    CHECK(queue.empty());

    CHECK(queue.try_push(1));
    CHECK(queue.try_push(2));
    CHECK(queue.try_push(3));
    CHECK_FALSE(queue.empty());

    int value = 0;
    REQUIRE(queue.try_pop(value));
    CHECK(value == 1);
    REQUIRE(queue.try_pop(value));
    CHECK(value == 2);
    REQUIRE(queue.try_pop(value));
    CHECK(value == 3);
    CHECK(queue.empty());
}

TEST_CASE("SpscQueue는 가득 차면 push를 거부하고, 비면 pop을 거부한다.") {
    SpscQueue<int> queue(3);

    CHECK(queue.try_push(1));
    CHECK(queue.try_push(2));
    CHECK(queue.try_push(3));
    CHECK_FALSE(queue.try_push(4));

    int value = 0;
    REQUIRE(queue.try_pop(value));
    CHECK(value == 1);

    // 하나를 비웠으니 다시 넣을 수 있고, 링이 감긴 뒤에도 순서가 유지된다.
    CHECK(queue.try_push(4));
    REQUIRE(queue.try_pop(value));
    CHECK(value == 2);
    REQUIRE(queue.try_pop(value));
    CHECK(value == 3);
    REQUIRE(queue.try_pop(value));
    CHECK(value == 4);
    CHECK_FALSE(queue.try_pop(value));
}

TEST_CASE("SpscQueue는 용량 1에서도 full과 empty를 구분한다.") {
    SpscQueue<int> queue(1);

    CHECK(queue.try_push(1));
    CHECK_FALSE(queue.try_push(2));

    int value = 0;
    REQUIRE(queue.try_pop(value));
    CHECK(value == 1);
    CHECK_FALSE(queue.try_pop(value));

    CHECK(queue.try_push(2));
    REQUIRE(queue.try_pop(value));
    CHECK(value == 2);
}

TEST_CASE("가득 찬 SpscQueue에 넣기 실패하면 인자를 move하지 않는다.") {
    SpscQueue<MoveTracker> queue(1);

    MoveTracker first{1};
    CHECK(queue.try_push(std::move(first)));
    CHECK(first.movedFrom());

    MoveTracker second{2};
    CHECK_FALSE(queue.try_push(std::move(second)));
    CHECK_FALSE(second.movedFrom());

    MoveTracker out;
    REQUIRE(queue.try_pop(out));
    CHECK(out.value() == 1);

    CHECK(queue.try_push(std::move(second)));
    CHECK(second.movedFrom());
}

TEST_CASE("SpscQueue는 Job을 담고 순서대로 꺼낸다.") {
    SpscQueue<Job> queue(2);
    JobCounter counter;
    int runs = 0;

    Job first{[&runs] { ++runs; }, &counter};
    Job second{[&runs] { runs += 10; }, &counter};
    CHECK(queue.try_push(std::move(first)));
    CHECK(queue.try_push(std::move(second)));

    Job out;
    REQUIRE(queue.try_pop(out));
    out.run();
    CHECK(runs == 1);

    REQUIRE(queue.try_pop(out));
    out.run();
    CHECK(runs == 11);
}

TEST_CASE("SpscQueue는 1 생산자/1 소비자 스트레스에서 유실과 순서 역전이 없다.") {
    constexpr std::size_t kCount = 100'000;

    SpscQueue<std::size_t> queue(64);
    std::vector<std::size_t> received;
    received.reserve(kCount);

    std::thread producer([&queue] {
        for (std::size_t i = 0; i < kCount; ++i) {
            while (!queue.try_push(std::size_t{i})) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&queue, &received] {
        std::size_t value = 0;
        while (received.size() < kCount) {
            if (queue.try_pop(value)) {
                received.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    std::vector<std::size_t> expected(kCount);
    std::iota(expected.begin(), expected.end(), std::size_t{0});
    CHECK(received == expected);
}
