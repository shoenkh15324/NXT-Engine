#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace nxt::core::jobs {

/**
 * @brief 제출된 잡의 완료 수를 세는 카운터이다.
 *
 * 제출자가 add()로 기대 완료 수를 올리고, 잡을 실행한 worker가 complete()로 하나씩
 * 내린다. 소유자는 wait()로 0이 될 때까지 기다린다.
 *
 * 소유권과 수명:
 * - 카운터는 호출자(제출자)가 소유한다. Job은 이 카운터를 가리키는 비소유 포인터만
 *   들고 있다.
 * - wait()가 끝나기 전에 카운터를 파괴하면 안 된다. 파괴 시점에 완료 수가 0이
 *   아니면 Debug에서 NXT_ASSERT가 중단한다.
 * - wait()가 반환한 뒤에는 complete()를 부르는 worker가 카운터를 더 이상 만지지
 *   않는다. 마지막 complete()가 mutex를 잡은 채로 감소와 통지를 끝내기 때문이다.
 *
 * 제출 순서:
 * worker가 감소시키기 전에 반드시 add()가 끝나 있어야 한다.
 * @code
 * counter.add(1);          // 1. 먼저 기대 완료 수를 올린다
 * queue.push(Job{...});    // 2. 그 다음 잡을 worker에게 보인다
 * @endcode
 * 큐 push가 실패하면 complete()로 되돌린다. 순서를 뒤집으면 worker가 먼저
 * 감소시켜 카운터가 underflow한다.
 *
 * @note Thread safety: add()와 wait()는 소유 스레드 하나만 부른다. complete()는
 *       여러 worker가 동시에 불러도 된다. pending()은 어느 스레드에서나 읽을 수
 *       있지만 스냅샷이다.
 */
class JobCounter final {
public:
    JobCounter() = default;
    ~JobCounter();

    JobCounter(const JobCounter&) = delete;
    JobCounter& operator=(const JobCounter&) = delete;
    JobCounter(JobCounter&&) = delete;
    JobCounter& operator=(JobCounter&&) = delete;

    /**
     * @brief 기대 완료 수를 올린다.
     * @param[in] count 올릴 수. 0이면 아무 일도 하지 않는다.
     *
     * @note 잡을 큐에 넣기 **전에** 부른다. 큐 push가 실패하면 complete()로 되돌린다.
     */
    void add(std::uint32_t count = 1) noexcept;

    /**
     * @brief 완료 하나를 기록한다. 잡을 실행한 worker가 부른다.
     *
     * 마지막 완료면 wait()로 잠든 소유자를 깨운다.
     *
     * @note 잡 하나당 정확히 한 번 불러야 한다. 제출 수보다 많이 부르면 계약 위반이며
     *       Debug에서 NXT_ASSERT가 중단한다.
     */
    void complete() noexcept;

    /**
     * @brief 완료 수가 0이 될 때까지 기다린다.
     *
     * 이미 0이면 즉시 반환한다. 반환 뒤에는 이 카운터에 연결된 잡이 모두 실행을 마쳤고,
     * 그 잡들이 쓴 결과를 안전하게 읽을 수 있다.
     *
     * @note 소유 스레드 하나만 부른다.
     */
    void wait() noexcept;

    /// @brief 지금 남은 완료 수를 반환한다. 스냅샷이며 대기 판단에 쓰지 않는다.
    [[nodiscard]]
    std::uint32_t pending() const noexcept;

private:
    /// @brief 아직 complete()되지 않은 잡 수이다. pending()이 lock 없이 읽는다.
    std::atomic<std::uint32_t> count_{0};

    /// @brief wait()와 complete()의 통지 프로토콜을 직렬화한다.
    std::mutex mutex_;

    /// @brief count_가 0이 되면 wait()를 깨운다.
    std::condition_variable completed_;
};

} // namespace nxt::core::jobs
