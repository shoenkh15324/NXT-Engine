#pragma once

#include <atomic>
#include <cstddef>
#include <nxt/core/diagnostics/assert.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace nxt::core::concurrency {

/**
 * @brief 생산자 하나와 소비자 하나가 공유하는 고정 용량 큐이다.
 *
 * 링 버퍼의 빈 슬롯 하나를 full/empty 구분에 쓴다. 그래서 실제 저장 가능한 수는
 * 생성자에 넘긴 capacity와 같고, 내부 버퍼는 capacity + 1칸이다.
 *
 * 계약:
 * - try_push()는 생산자 스레드 하나만, try_pop()은 소비자 스레드 하나만 부른다.
 * - try_push()가 false면 큐가 가득 찬 것이고, 인자는 move되지 않는다.
 * - try_pop()이 false면 큐가 비어 있고, out은 바뀌지 않는다.
 * - empty()는 스냅샷이다. 동시에 바뀔 수 있으므로 대기 판단에 쓰지 않는다.
 *
 * @note T는 기본 생성 가능하고 예외를 던지지 않는 move 대입을 가져야 한다.
 *       링 버퍼가 capacity + 1개의 T를 미리 만들어 두기 때문이다.
 * @note Thread safety: 생산자/소비자 각각 한 스레드만 접근한다. 그 외 스레드가
 *       try_push()나 try_pop()을 부르면 안 된다.
 */
template <typename T>
class SpscQueue final {
public:
    static_assert(std::is_default_constructible_v<T>, "SpscQueue는 기본 생성 가능한 T만 지원한다.");
    static_assert(std::is_nothrow_move_assignable_v<T>, "SpscQueue는 예외를 던지지 않는 move 대입만 지원한다.");
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "SPSC 큐 인덱스는 lock-free atomic이어야 한다.");

    /**
     * @brief 큐를 만든다.
     *
     * @parma[in] capacity 저장할 수 있는 최대 원소 수. 1 이상이어야 한다.
     */
    explicit SpscQueue(std::size_t capacity) : buffer_(capacity + 1) {
        NXT_ASSERT(capacity > 0);
    }

    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = delete;
    SpscQueue& operator=(SpscQueue&&) = delete;

    /**
     * @brief 원소 하나를 넣는다.
     *
     * @param[in] value 넣을 원소. 성공하면 move된다.
     * @return 넣었으면 true, 가득차서 넣지 못했으면 false.
     *
     * @note false를 돌려줄 때 value는 move되지 않는다. 호출자가 재시도하거나
     *       되돌릴 수 있다.
     */
    [[nodiscard]]
    bool try_push(T&& value) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = nextIndex(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[head] = std::move(value);
        // 슬롯에 쓴 데이터를 소비자에게 공개한다. 이 store 이전의 write는
        // 소비자의 acquire load 뒤에 보인다.
        head_.store(next, std::memory_order_release);
        return true;
    }

    /**
     * @brief 원소 하나를 꺼낸다.
     *
     * @param[out] out 꺼낸 원소를 move 대입할 대상. 큐가 비어 있으면 바뀌지 않는다.
     * @return 꺼냈으면 true, 비어 있으면 false.
     */
    [[nodiscard]]
    bool try_pop(T& out) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        out = std::move(buffer_[tail]);
        // 슬롯을 다 읽었다는 사실을 생산자에게 공개한다. 이 store 이전의 read는
        // 생산자의 acquire load 뒤에 보인다.
        tail_.store(nextIndex(tail), std::memory_order_release);
        return true;
    }

    /// @brief 지금 비어 있는지 반환한다. 스냅샷이며 동시에 바뀔 수 있다.
    [[nodiscard]]
    bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    /// @brief 저장 가능한 최대 원소 수를 반환한다.
    [[nodiscard]]
    std::size_t capacity() const noexcept {
        return buffer_.size() - 1;
    }

private:
    /// @brief index 다음 칸. 마지막 칸이면 0으로 돌아간다.
    [[nodiscard]]
    std::size_t nextIndex(std::size_t index) const noexcept {
        const std::size_t next = index + 1;
        return (next == buffer_.size()) ? 0 : next;
    }

    /// @brief capacity + 1칸짜리 링 버퍼이다. 빈 슬롯 하나로 full/empty를 구분한다
    std::vector<T> buffer_;

    /// @brief 생산자가 쓰는 다음 슬롯이다. 소비자와 캐시 라인을 분리한다.
    alignas(64) std::atomic<std::size_t> head_{0};

    /// @brief 소비자가 읽을 다음 슬롯이다. 생산자와 캐시 라인을 분리한다.
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace nxt::core::concurrency
