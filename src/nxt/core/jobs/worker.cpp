#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/jobs/job_counter.hpp>
#include <nxt/core/jobs/worker.hpp>
#include <utility>

namespace nxt::core::jobs {

Worker::Worker(std::size_t queueCapacity) : queue_(queueCapacity) {}

Worker::~Worker() {
    // 실행 중에 파괴하면 워커 스레드만 이미 파괴된 큐와 신호를 만진다. Release에서는
    // assert가 사라지지만, std 구현은 join 없는 Thread 소멸에서 terminate하므로
    // 계약 위반이 조용히 넘어가지 않는다.
    NXT_ASSERT(thread_ == nullptr);
}

void Worker::start(ThreadSource& source) {
    NXT_ASSERT(thread_ == nullptr);
    NXT_ASSERT(!stop_.load(std::memory_order_acquire));

    thread_ = source.start([this] { loop(); });
    NXT_ASSERT(thread_ != nullptr);
}

void Worker::stop() noexcept {
    // 이미 멈췄으면 신호를 또 울리지 않는다. join() 뒤에 불러도 안전한다.
    if (stop_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    // 잠든 워커를 깨운다. 값을 올린 뒤에 통지해야 wait()가 바뀐 값을 보고 깨어난다.
    signal_.fetch_add(1, std::memory_order_release);
    signal_.notify_all();
}

void Worker::join() {
    NXT_ASSERT(thread_ != nullptr);
    NXT_ASSERT(stop_.load(std::memory_order_acquire));

    if (!thread_) {
        return;
    }

    thread_->join();
    thread_.reset();
}

bool Worker::trySubmit(Job&& job) noexcept {
    NXT_ASSERT(!stop_.load(std::memory_order_acquire));

    // 가득 차면 큐가 인자를 move하지 않는다. 호출자가 같은 잡으로 재시도할 수 있다.
    if (!queue_.try_push(std::move(job))) {
        return false;
    }

    // push를 release로 끝낸 뒤 신호를 올린다. 워커는 신호를 먼저 읽고 큐를 보므로,
    // 이 순서가 아니면 잡을 넣고도 깨우지 못한 채 잠들 수 있다.
    signal_.fetch_add(1, std::memory_order_release);
    signal_.notify_one();
    return true;
}

bool Worker::running() const noexcept {
    return thread_ != nullptr;
}

void Worker::loop() noexcept {
    for (;;) {
        // 신호를 큐보다 먼저 읽는다. 워커가 신호를 읽은 뒤 큐를 확인하고 잠들기까지
        // 생산자가 push와 신호 증가를 마쳤다면, wait()는 바뀐 값을 보고 즉시 깨어난다.
        // 반대 순서(큐 확인 -> 신호 읽기)는 그 사이에 들어온 잡을 놓친 채 잠든다.
        const std::uint32_t signal = signal_.load(std::memory_order_acquire);

        Job job;
        if (queue_.try_pop(job)) {
            runJob(job);
            continue;
        }

        if (stop_.load(std::memory_order_acquire)) {
            // 종료 요청이 와도 이미 큐에 들어온 잡은 버리지 않는다. 버리면 그 잡으
            // JobCounter가 영원히 0이 되지 않아 소유자의 wait()가 멈춘다.
            while (queue_.try_pop(job)) {
                runJob(job);
            }
            return;
        }

        signal_.wait(signal, std::memory_order_acquire);
    }
}

void Worker::runJob(Job& job) noexcept {
    // 빈 함수나 null 카운터는 제출자가 계약을 어긴 것이다. Release에서는 assert가
    // 사라지므로 이 경우는 빈 함수 호출로 terminate되거나 카운터가 멈춘다.
    NXT_ASSERT(job.valid());

    // 실행이 끝난 뒤에 완료를 알린다. 순서를 뒤집으면 소유자가 결과를 읽기 전에
    // wait()에서 깨어난다.
    job.run();
    job.counter->complete();
}

} // namespace nxt::core::jobs
