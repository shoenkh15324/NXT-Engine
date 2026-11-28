#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <nxt/core/concurrency/spsc_queue.hpp>
#include <nxt/core/jobs/job.hpp>
#include <nxt/core/jobs/thread_source.hpp>

namespace nxt::core::jobs {

/**
 * @brief 자기 큐 하나를 소비하는 작업 스레드이다.
 *
 * 제출자(스케줄러)가 trySubmit()으로 잡을 넣으면 워커 스레드가 큐에서 꺼내 실행하고
 * JobCounter::complete()를 부른다. 큐가 비면 잡이 들어오거나 stop()이 불릴 때까지
 * 잠든다.
 *
 * 계약:
 * - trySubmit()은 생산자 스레드 하나만, 실행 루프는 워커 스레드 하나만 큐를 만진다.
 * - trySubmit()은 큐가 가득 차면 false를 돌려주고 인자를 move하지 않는다.
 * - 잡 하나당 complete()를 정확히 한 번 부른다. 그래서 stop()이 와도 이미 제출한
 *   잡은 버려지지 않는다.
 * - stop()은 큐에 남은 잡을 모두 실행한 뒤 루프를 끝낸다.
 *
 * 수명:
 * - start()에 넘긴 ThreadSource는 join()이 끝날 때까지 살아 있어야 한다.
 * - Worker는 start() 전이나 join() 뒤에 파괴한다. 실행 중에 파괴하면 Debug에서
 *   NXT_ASSERT가 중단한다.
 *
 * @note 재시작하지 않는다. start()는 Worker마다 한 번, join()도 한 번 부른다.
 * @note 잡 함수는 예외를 던지면 안 된다. 워커 스레드에서 예외가 빠져나가면
 *       std::terminate된다.
 */
class Worker final {
public:
    /**
     * @brief 큐만 만들고 스레드는 만들지 않는다.
     *
     * @param[in] queueCapacity 큐에 넣을 수 있는 최대 잡 수. 1 이상이어야 한다.
     */
    explicit Worker(std::size_t queueCapacity);

    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&) = delete;
    Worker& operator=(Worker&&) = delete;

    /**
     * @brief source로 워커 스레드를 만들고 실행 루프를 시작한다.
     *
     * @param[in] source 스레드를 만드는 source. join()까지 살아 있어야 한다.
     */
    void start(ThreadSource& source);

    /**
     * @brief 실행 루프에 종료를 요청한다. 큐에 남은 잡은 모두 실행된다.
     *
     * 두 번 불러도 안전하다. join() 뒤에 불러도 된다.
     */
    void stop() noexcept;

    /**
     * @brief 워커 스레드가 끝날 때까지 기다린다. join() 뒤 running()은 false이다
     *
     * @note stop()을 먼저 부른다. stop() 없이 부르면 루프가 끝나지 않아 멈춘다.
     */
    void join();

    /**
     * @brief 잡 하나를 큐에 넣는다.
     *
     * @param[in] job 넣을 잡. 성공하면 move된다.
     * @return 넣었으면 true, 큐가 가득차면 false.
     *
     * @note stop() 뒤에는 부르지 않는다.
     */
    [[nodiscard]]
    bool trySubmit(Job&& job) noexcept;

    /// @brief 워커 스레드가 살아 있는지 반환한다.
    [[nodiscard]]
    bool running() const noexcept;

private:
    /// @brief 워커 스레드의 실행 루프이다.
    void loop() noexcept;

    /// @brief 잡 하나를 실행하고 완료를 알린다.
    void runJob(Job& job) noexcept;

    /// @brief 생산자가 넣고 워커가 꺼내는 잡 큐이다.
    concurrency::SpscQueue<Job> queue_;

    /// @brief 워커 스레드이다. join() 뒤에는 null이 된다.
    std::unique_ptr<Thread> thread_;

    /// @brief 종료 요청이다. stop()에서만 true가 된다.
    std::atomic<bool> stop_{false};

    /**
     * @brief 잡 도착과 종료 요청을 워커에게 알리는 신호이다.
     *
     * 값 자체에는 의미가 없다. 바뀐 사실만 wait()를 깨운다.
     */
    std::atomic<std::uint32_t> signal_{0};
};

} // namespace nxt::core::jobs
