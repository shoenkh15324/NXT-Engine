#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/jobs/std_thread_source.hpp>
#include <thread>
#include <utility>

namespace nxt::core::jobs {

namespace {

/**
 * @brief std::thread를 소유하는 Thread 구현이다.
 *
 * @note 소유권: std::thread를 값으로 들고 있으므로 이동할 수 없다. 호출자는
 *       unique_ptr로 소유한다.
 */
class StdThread final : public Thread {
public:
    explicit StdThread(ThreadFunction body)
        : thread_([body = std::move(body)] {
              if (body) {
                  body();
              }
          }) {}

    ~StdThread() override {
        // join 없이 소멸하면 std::thread 소멸자가 terminate한다. 그 전에 계약 위반을
        // 드러내 Debug에서 원인을 바로 볼 수 있게 한다. Release에서는 assert가 사라지고
        // std::thread의 기본 동작(terminate)이 남는다.
        NXT_ASSERT(!thread_.joinable());
    }

    void join() override {
        NXT_ASSERT(thread_.joinable());

        // 두 번 join하면 std::thread가 예외를 던진다(예외를 끈 빌드에서는 terminate).
        // Release에서는 assert가 사라지므로 joinable을 확인하고 넘어간다.
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    std::thread thread_;
};

} // namespace

std::unique_ptr<Thread> StdThreadSource::start(ThreadFunction body) {
    // 빈 body는 프로그래머 오류다. Release에서는 스레드가 즉시 끝난다.
    NXT_ASSERT(body);
    return std::make_unique<StdThread>(std::move(body));
}

} // namespace nxt::core::jobs
