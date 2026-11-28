#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/jobs/worker_pool.hpp>
#include <thread>

namespace nxt::core::jobs {

std::size_t defaultWorkerCount() noexcept {
    return normalizeWorkerCount(std::thread::hardware_concurrency());
}

WorkerPool::WorkerPool(std::size_t workerCount, std::size_t queueCapacity) {
    // 0개 pool은 잡을 받아도 실행할 worker가 없다. 호출자가 보정하지 않은 것이다.
    NXT_ASSERT(workerCount > 0);

    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) {
        workers_.push_back(std::make_unique<Worker>(queueCapacity));
    }
}

WorkerPool::~WorkerPool() {
    // stop()/join() 없이 파괴하면 worker 스레드가 이미 파괴된 큐를 만진다.
    // Release에서는 assert가 사라지지만, std 구현은 join 없는 Thread 소멸에서
    // terminate하므로 계약 위반이 조용히 넘어가지는 않는다.
    NXT_ASSERT(!started_);
}

void WorkerPool::start(ThreadSource& source) {
    NXT_ASSERT(!started_);
    NXT_ASSERT(!workers_.empty());

    for (const std::unique_ptr<Worker>& worker : workers_) {
        worker->start(source);
    }

    started_ = true;
}

void WorkerPool::stop() noexcept {
    NXT_ASSERT(started_);

    // 각 worker는 큐에 남은 잡을 실행한 뒤 루프를 끝낸다.
    for (const std::unique_ptr<Worker>& worker : workers_) {
        worker->stop();
    }
}

void WorkerPool::join() {
    NXT_ASSERT(started_);

    for (const std::unique_ptr<Worker>& worker : workers_) {
        worker->join();
    }

    started_ = false;
}

Worker& WorkerPool::worker(std::size_t index) noexcept {
    NXT_ASSERT(index < workers_.size());
    return *workers_[index];
}

const Worker& WorkerPool::worker(std::size_t index) const noexcept {
    NXT_ASSERT(index < workers_.size());
    return *workers_[index];
}

std::size_t WorkerPool::workerCount() const noexcept {
    return workers_.size();
}

bool WorkerPool::running() const noexcept {
    return started_;
}

} // namespace nxt::core::jobs
