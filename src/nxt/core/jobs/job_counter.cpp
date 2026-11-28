#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/jobs/job_counter.hpp>

namespace nxt::core::jobs {

JobCounter::~JobCounter() {
    // 소유자가 wait() 없이 파괴하면 worker가 이미 파괴된 카운터를 만질 수 있다.
    // Release에서는 assert가 사라지므로 계약은 주석과 리뷰로 지킨다.
    NXT_ASSERT_MSG(count_.load(std::memory_order_acquire) == 0, "JobCounter가 남은 잡을 기다리지 않고 파괴됐다.");
}

void JobCounter::add(std::uint32_t count) noexcept {
    if (count == 0) {
        return;
    }

    // add()와 complete()가 같은 mutex를 쓰므로 감소와 증가가 섞이지 않는다.
    // 값을 올리는 것으로는 wait()의 조건이 참이 되지 않으므로 notify는 필요 없다.
    const std::lock_guard<std::mutex> lock(mutex_);
    count_.fetch_add(count, std::memory_order_relaxed);
}

void JobCounter::complete() noexcept {
    // notify를 mutex 안에서 부른다. wait()가 반환한 시점에 마지막 complete()가
    // 카운터 접근을 모두 끝냈음을 보장하기 위해서다. notify를 lock 밖에서 부르면
    // wait()가 먼저 반환해 소유자가 카운터를 파괴할 수 있다.
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::uint32_t previous = count_.fetch_sub(1, std::memory_order_acq_rel);
    NXT_ASSERT_MSG(previous > 0, "JobCounter의 완료 수가 제출 수보다 많다.");
    if (previous == 1) {
        completed_.notify_one();
    }
}

void JobCounter::wait() noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_.wait(lock, [this] { return count_.load(std::memory_order_acquire) == 0; });
}

std::uint32_t JobCounter::pending() const noexcept {
    return count_.load(std::memory_order_acquire);
}

} // namespace nxt::core::jobs
