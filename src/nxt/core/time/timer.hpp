#pragma once

#include <cstdint>
#include <nxt/core/time/time.hpp>

namespace nxt::time {

/**
 * @brief 구간 시간을 재는 값 타입이다.
 *
 * 생성 시 측정을 시작하고 stop()/start()로 일시정지할 수 있다. 재개해도 이미
 * 소비한 시간은 누적되므로, 멈춰 있는 동안의 시간은 측정 대상이 아니다.
 *
 * 플랫폼 레이어의 타이머 소스와 무관하다. 시작 시점의 Timestamp와
 * 누적된 Duration으로 상태를 표현하며, 시간 측정 자체는 time 계층의
 * 시계 소스를 사용한다.
 *
 * @note Thread safety: thread-safe하지 않다. 한 thread가 소유해 사용하거나
 *       호출자가 외부에서 직렬화한다. 소유권은 Timer를 값으로 가진 쪽에 있다.
 */
class Timer {
public:
    /// 생성과 동시에 측정을 시작한다.
    Timer() noexcept : start_{now()} {}

    /// 측정을 시작한다. 이미 실행 중이면 아무 일도 하지 않는다.
    void start() noexcept {
        if (running_) {
            return;
        }
        start_ = now();
        running_ = true;
    }

    /// 현재 구간을 누적한 뒤 멈춘다. 이미 멈춰 있으면 아무 일도 하지 않는다.
    void stop() noexcept {
        if (!running_) {
            return;
        }
        accumulated_ += now() - start_;
        running_ = false;
    }

    /// 누적 시간을 0으로 만든다. 실행 중이었다면 지금부터 다시 잰다.
    void reset() noexcept {
        accumulated_ = Duration{};
        start_ = now();
    }

    /// @brief 측정 중인지 여부를 반환한다.
    [[nodiscard]]
    bool running() const noexcept {
        return running_;
    }

    /**
     * @brief 누적된 경과 시간을 nanosecond 단위로 반환한다.
     *
     * 멈춰 있으면 누적값에서 더 이상 증가하지 않는다.
     */
    [[nodiscard]]
    std::uint64_t elapsedNs() const noexcept {
        // 측정 중인 경우 현재 시각을 한 번만 읽어 스냅샷을 만든다.
        Duration total = accumulated_;
        if (running_) {
            total += now() - start_;
        }
        return total.ns();
    }

    /**
     * @brief 누적된 경과 시간을 microsecond 단위로 반환한다.
     *
     * 소수 부분은 버린다.
     */
    [[nodiscard]]
    std::uint64_t elapsedUs() const noexcept {
        return elapsedNs() / kNsPerUs;
    }

    /**
     * @brief 누적된 경과 시간을 millisecond 단위로 반환한다.
     *
     * 소수 부분은 버린다.
     */
    [[nodiscard]]
    std::uint64_t elapsedMs() const noexcept {
        return elapsedNs() / kNsPerMs;
    }

private:
    /// 현재 측정 구간의 시작 지점. stopped 상태에서는 사용하지 않는다.
    Timestamp start_;
    /// 끝난 구간들의 합.
    Duration accumulated_;
    bool running_{true};
};

} // namespace nxt::time
