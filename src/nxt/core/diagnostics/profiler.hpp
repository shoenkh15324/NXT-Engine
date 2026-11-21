#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <nxt/core/time/time.hpp>
#include <optional>
#include <span>

namespace nxt::core::diag {

/**
 * @brief 한 구간의 이름과 소요 시간이다.
 *
 * @note name은 문자열 리터럴을 가리킨다. Profiler가 복사하지 않으므로 수명이
 *       짧은 문자열을 넘기면 안 된다.
 */
struct Sample {
    /// @brief 구간 이름이다. 로그에 그대로 나온다.
    const char* name{nullptr};

    /// @brief 구간의 소요 시간(나노초)이다.
    std::uint64_t ns{0};
};

/// @brief 한 프레임에 기록할 수 있는 구간 수이다.
inline constexpr std::size_t kMaxSamplesPerFrame = 8;

/// @brief 한 프레임의 구간 표본 배열이다.
using FrameSamples = std::array<Sample, kMaxSamplesPerFrame>;

/**
 * @brief 프레임 안의 CPU 구간을 재는 계측기이다.
 *
 * `NXT_PROFILE_SCOPE("cull")`가 만드는 Scope가 이 객체에 시작·종료를 기록한다.
 * draw마다 재지 않는다 — 프레임의 굵은 구간만 잰다 (phase1.md §5.1).
 *
 * 비활성 상태에서 Scope는 atomic load 하나만 하고 끝난다. 그래서 1-C5에서
 * "계측 때문에 느려졌는가"를 같은 자로 잴 수 있다 (phase1.md §5.6).
 *
 * @note 수명을 가진 객체다. FrameLoop가 소유하고 소멸 전에 setCurrent(nullptr)로
 *       해제한다.
 * @note Thread safety: enabled 플래그만 atomic이다. 나머지는 한 스레드에서만 쓴다.
 *       Phase 1은 프레임 기록이 메인 스레드 하나다.
 */
class Profiler final {
public:
    /// @brief 구간을 기록하지 못했을 때 beginScope()가 돌려주는 값이다.
    static constexpr std::size_t kNoScope = static_cast<std::size_t>(-1);

    Profiler() = default;

    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;

    /// @brief 계측이 켜져 있는지 반환한다. Scope가 한 번만 읽는다.
    [[nodiscard]]
    bool enabled() const noexcept;

    /**
     * @brief 계측을 켜고 끈다.
     *
     * @param[in] enabled true면 이후 Scope가 표본을 남긴다.
     */
    void setEnabled(bool enabled) noexcept;

    /// @brief 이번 프레임의 표본을 비우고 기록을 시작한다.
    void beginFrame() noexcept;

    /// @brief 이번 프레임의 기록을 닫는다. 표본은 samples()로 계속 읽을 수 있다.
    void endFrame() noexcept;

    /// @brief 이번 프레임에 기록된 구간 표본을 반환한다.
    [[nodiscard]]
    std::span<const Sample> samples() const noexcept;

    /**
     * @brief 이 스레드에서 Scope가 쓸 Profiler를 등록한다.
     *
     * @param[in] profiler 등록할 Profiler. nullptr이면 해제한다.
     */
    static void setCurrent(Profiler* profiler) noexcept;

    /// @brief 이 스레드에 등록된 Profiler를 반환한다. 없으면 nullptr이다.
    [[nodiscard]]
    static Profiler* current() noexcept;

    /**
     * @brief 구간 표본을 하나 연다. Scope 전용이다.
     *
     * @param[in] name 구간 이름. 문자열 리터럴이어야 한다.
     * @return 표본 인덱스. 기록할 수 없으면 kNoScope.
     */
    [[nodiscard]]
    std::size_t beginScope(const char* name) noexcept;

    /**
     * @brief 열어 둔 구간을 닫고 소요 시간을 채운다. Scope 전용이다.
     *
     * @param[in] index beginScope()가 돌려준 인덱스.
     * @param[in] ns    구간의 소요 시간.
     */
    void endScope(std::size_t index, std::uint64_t ns) noexcept;

private:
    /// @brief 비활성 경로에서도 읽는 유일한 상태이다.
    std::atomic<bool> enabled_{false};

    /// @brief 이번 프레임의 구간 표본이다.
    FrameSamples samples_{};

    /// @brief samples_에 채워진 표본 수이다.
    std::size_t count_{0};

    /// @brief beginFrame()과 endFrame() 사이인지 나타낸다.
    bool frameActive_{false};
};

/**
 * @brief 생성자와 소멸자 사이의 시간을 Profiler에 기록하는 RAII 객체이다.
 *
 * `NXT_PROFILE_SCOPE("record")`가 이 타입을 만든다. 계측이 꺼져 있거나 현재
 * Profiler가 없으면 아무 일도 하지 않는다.
 *
 * @note 복사·이동할 수 없다. 구간 하나가 Scope 하나다.
 */
class Scope final {
public:
    /**
     * @brief 현재 Profiler에 구간을 연다.
     *
     * @param[in] name 구간 이름. 문자열 리터럴이어야 한다.
     */
    explicit Scope(const char* name) noexcept;

    /// @brief 구간을 닫고 소요 시간을 기록한다.
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

private:
    /// @brief 이 구간을 기록할 Profiler이다. nullptr이면 비활성 경로이다.
    Profiler* profiler_{nullptr};

    /// @brief 열어 둔 표본 인덱스이다.
    std::size_t index_{Profiler::kNoScope};

    /// @brief 구간 시작 시각이다. 비활성 경로에서는 비어 있다.
    std::optional<time::Timestamp> start_{};
};

} // namespace nxt::core::diag

/// @brief 이름 붙은 구간을 만든다. 같은 이름의 scope가 중첩돼도 된다.
#define NXT_PROFILE_CONCAT_INNER(left, right) left##right
#define NXT_PROFILE_CONCAT(left, right) NXT_PROFILE_CONCAT_INNER(left, right)
#define NXT_PROFILE_SCOPE(name)                                                                                        \
    ::nxt::core::diag::Scope NXT_PROFILE_CONCAT(nxtProfileScope_, __LINE__) {                                          \
        name                                                                                                           \
    }
