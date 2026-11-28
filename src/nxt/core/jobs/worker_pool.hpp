#pragma once

#include <cstddef>
#include <memory>
#include <nxt/core/jobs/thread_source.hpp>
#include <nxt/core/jobs/worker.hpp>
#include <vector>

namespace nxt::core::jobs {

/**
 * @brief hardware_concurrency()가 0을 돌려주는 경우를 1로 보정한다.
 *
 * std::thread::hardware_concurrency()는 값을 알 수 없으면 0을 돌려준다. worker가
 * 0개면 제출한 잡이 영원히 실행되지 않으므로 최소 1개로 본다.
 *
 * @param[in] hardwareConcurrency hardware_concurrency()가 돌려준 값.
 * @return 1 이상의 worker 수.
 */
[[nodiscard]]
constexpr std::size_t normalizeWorkerCount(std::size_t hardwareConcurrency) noexcept {
    return hardwareConcurrency == 0 ? 1 : hardwareConcurrency;
}

/**
 * @brief 이 기계에서 쓸 기본 worker 수이다.
 *
 * normalizeWorkerCount(std::thread::hardware_concurrency())와 같다.
 *
 * @return 1 이상의 worker 수.
 */
[[nodiscard]]
std::size_t defaultWorkerCount() noexcept;

/**
 * @brief N개 worker를 만들고 시작·정지·join을 한 번에 관리한다.
 *
 * 생성자는 큐만 만든다. start()가 source로 스레드를 만들고, stop()이 모든 worker에
 * 종료를 요청하며, join()이 모두 끝날 때까지 기다린다.
 *
 * 계약:
 * - start() → stop() → join() 순서로 정확히 한 번씩 부른다.
 * - stop()은 큐에 남은 잡을 버리지 않는다. 각 worker가 남은 잡을 실행한 뒤 끝난다.
 * - join() 전에는 pool을 파괴하지 않는다. Debug에서는 소멸자가 assert로 막는다.
 * - 잡 제출은 worker(index).trySubmit()으로 한다. 어느 worker에 넣을지는 호출자가
 *   정한다(1-B6 Scheduler의 라운드 로빈).
 *
 * @note Thread safety: start()/stop()/join()과 worker()는 소유 스레드 하나만 부른다.
 *       worker(index).trySubmit()도 같은 스레드에서만 부른다.
 * @note start() 도중 스레드 생성이 실패하면(std::system_error) 예외가 그대로
 *       빠져나간다. core는 예외를 잡지 않는다(CODING_RULES §13).
 */
class WorkerPool final {
public:
    /**
     * @brief worker와 그 큐를 만든다. 스레드는 start()에서 만든다.
     *
     * @param[in] workerCount worker 수. 1 이상이어야 한다.
     * @param[in] queueCapacity worker마다 둘 큐의 용량. 1 이상이어야 한다.
     */
    WorkerPool(std::size_t workerCount, std::size_t queueCapacity);

    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&) = delete;
    WorkerPool& operator=(WorkerPool&&) = delete;

    /**
     * @brief worker 수만큼 스레드를 만들고 실행 루프를 시작한다.
     *
     * @param[in] source 스레드를 만드는 source. join()까지 살아 있어야 한다.
     */
    void start(ThreadSource& source);

    /// @brief 모든 worker에 종료를 요청한다. 큐에 남은 잡은 실행된다.
    void stop() noexcept;

    /// @brief 모든 worker 스레드가 끝날 때까지 기다린다.
    void join();

    /**
     * @brief index번째 worker를 반환한다.
     *
     * @param[in] index 0 이상 workerCount() 미만
     */
    [[nodiscard]]
    Worker& worker(std::size_t index) noexcept;

    /// @brief index번째 worker를 반환한다.
    [[nodiscard]]
    const Worker& worker(std::size_t index) const noexcept;

    /// @brief worker 수를 반환한다.
    [[nodiscard]]
    std::size_t workerCount() const noexcept;

    /// @brief start()와 join() 사이인지 반환한다.
    [[nodiscard]]
    bool running() const noexcept;

private:
    /// @brief 이 pool이 소유한 worker들이다. 생성 순서가 index 순서이다.
    std::vector<std::unique_ptr<Worker>> workers_;

    /// @brief start()와 join() 사이인지 여부이다.
    bool started_{false};
};

} // namespace nxt::core::jobs
