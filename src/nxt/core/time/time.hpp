#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>

namespace nxt::time {

/// @brief nanosecond 단위 변환에 쓰이는 nanosecond 개수다.
inline constexpr std::uint64_t kNsPerUs = 1000u;

/// @brief nanosecond 단위 변환에 쓰이는 millisecond의 nanosecond 개수다.
inline constexpr std::uint64_t kNsPerMs = 1000u * kNsPerUs;

class Duration;

/**
 * @brief 모노토닉 시계 위의 한 지점이다.
 *
 * 절대값은 epoch가 임의이므로 해석하지 않는다. 의미 있는 연산은 두 Timestamp의
 * 차이뿐이고, 그 결과는 항상 Duration다.
 * 실수 초가 필요해지면 double로 변환한다. epoch 기반 값을 float로 올리면 정밀도가
 * 손실되므로, 호출자가 시작 시점을 뺀 상대값을 만들어야 한다.
 */
class Timestamp {
public:
    /// 비교는 defaulted 연산자로부터 파생된다.
    constexpr auto operator<=>(const Timestamp&) const noexcept = default;

private:
    friend constexpr Duration operator-(Timestamp lhs, Timestamp rhs) noexcept;
    friend Timestamp now() noexcept;

    /// now() 외에는 지점을 만들 수 없게 해 임의 값의 Timestamp를 막는다.
    explicit constexpr Timestamp(std::uint64_t ns) noexcept : ns_{ns} {}

    std::uint64_t ns_;
};

/**
 * @brief 시간의 양이다.
 *
 * 단위는 nanosecond로 고정한다.
 * Timestamp와 섞이지 않게 기본값(0)과 누적만 허용하고, 생성은 두 Timestamp의
 * 차이로만 이뤄진다.
 */
class Duration {
public:
    /// 0인 Duration로 초기화한다.
    constexpr Duration() noexcept = default;

    /// @brief nanosecond 단위의 시간을 반환한다.
    [[nodiscard]]
    constexpr std::uint64_t ns() const noexcept {
        return ns_;
    }

    /// @brief 다른 Duration을 누적한다.
    constexpr Duration& operator+=(Duration other) noexcept {
        ns_ += other.ns_;
        return *this;
    }

private:
    friend constexpr Duration operator-(Timestamp lhs, Timestamp rhs) noexcept;

    explicit constexpr Duration(std::uint64_t ns) noexcept : ns_{ns} {}

    std::uint64_t ns_{0};
};

/**
 * @brief 두 시점 사이의 시간을 반환한다.
 *
 * 역순 인자는 호출자 버그다. 모노토닉 시계는 전진만 하므로 NTP 조정 등으로
 * 역전되지 않는다.
 */
[[nodiscard]]
constexpr Duration operator-(Timestamp lhs, Timestamp rhs) noexcept {
    NXT_ASSERT(lhs >= rhs);
    return Duration{lhs.ns_ - rhs.ns_};
}

/**
 * @brief 현재 모노토닉 시점을 반환한다.
 *
 * steady_clock은 Windows에서 QPC, Linux에서 CLOCK_MONOTONIC 기반이라 플랫폼 전용
 * 타이머와 해상도 차이가 없다. 플랫폼 소스가 꼭 필요해지면 platform/time/가 이
 * 결과를 Timestamp로 만들어 내고 Timestamp와 Duration은 그대로 둔다.
 * Core는 그 존재를 알지 않는다.
 *
 * @note Thread safety: 상태가 없는 자유 함수이므로 thread-safe하다.
 */
[[nodiscard]]
inline Timestamp now() noexcept {
    const auto ticks =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
    // 대상 플랫폼의 steady_clock epoch는 부팅 시각이라 음수가 아니다. 음수가
    // 나오면 부호 없는 저장이 깨지므로 호출자가 알아야 한다.
    NXT_ASSERT(ticks.count() >= 0);
    return Timestamp{static_cast<std::uint64_t>(ticks.count())};
}

} // namespace nxt::time
