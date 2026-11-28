#pragma once

#include <functional>

namespace nxt::core::jobs {

class JobCounter;

/// @brief 잡 하나가 실행할 함수이다.
using JobFunction = std::function<void()>;

/**
 * @brief 실행 단위 하나이다.
 *
 * Worker가 큐에서 꺼내 run을 실행한 뒤 counter->complete()를 부른다. Job은 실행
 * 함수와 카운터를 빌려 쓸 뿐 소유하지 않는다. 둘 다 이 Job이 실행을 마칠 때까지
 * 살아 있어야 한다.
 *
 * @note 복사보다 이동을 쓴다. 큐에 넣을 때 소유권을 넘기면 수명을 읽기 쉽다.
 * @note Thread safety: Job 자체는 thread-safe하지 않다. 큐에 넣기 전까지는
 *       제출자만 만지고, 큐에 들어간 뒤에는 worker 하나만 만진다.
 */
struct Job {
    /// @brief 실행할 함수이다. 비어 있으면 안 된다.
    JobFunction run;

    /**
     * @brief 이 잡의 완료를 알릴 카운터이다. null이면 안 된다.
     *
     * @note 소유하지 않는다. 제출자가 소유하며, 이 잡이 완료를 알릴 때까지 살아
     *       있어야 한다.
     */
    JobCounter* counter{nullptr};

    /// @brief 실행 함수와 카운터가 모두 있는지 반환한다.
    [[nodiscard]]
    bool valid() const noexcept {
        return static_cast<bool>(run) && (counter != nullptr);
    }
};

} // namespace nxt::core::jobs
