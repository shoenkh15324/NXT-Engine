#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <nxt/core/diagnostics/profiler.hpp>

namespace nxt::engine {

/**
 * @brief 한 프레임의 지표이다.
 *
 * CPU와 GPU는 다른 경로로 잰다 (phase1.md §6.1). CPU는 프레임 시작에서 submit
 * 반환이 끝나는 지점까지이고, GPU는 command buffer에 찍은 timestamp 두 개의
 * 차이다. 두 값을 같이 봐야 "어느 쪽이 밀리는가"를 말할 수 있다.
 *
 * @note 값 타입이다. FrameLoop가 표본으로 복사해 링 버퍼에 넣는다.
 */
struct FrameSample {
    /// @brief 프레임 시작 - submit 반환까지의 시간(나노초)이다.
    std::uint64_t cpuFrameNs{0};

    /// @brief GPU timestamp start - end의 시간(나노초)이다. 0이면 못 읽었다.
    std::uint64_t gpuFrameNs{0};

    /// @brief 이 레인의 fence를 기다린 시간(나노초)이다.
    std::uint64_t fenceWaitNs{0};

    /// @brief 이 프레임에 기록한 draw call 수이다.
    std::uint32_t drawCalls{0};

    /// @brief timestamp 미지원이라 fence wait로 GPU 시간을 대신했는지 여부이다.
    bool gpuApproximate{false};

    /// @brief 이 프레임의 CPU 구간 표본이다.
    core::diag::FrameSamples segments{};

    /// @brief segments에 채워진 구간 수이다.
    std::size_t segmentCount{0};
};

/**
 * @brief 한 CPU 구간의 중앙값과 p95이다.
 *
 * @note name은 문자열 리터럴을 가리킨다. 표본에서 그대로 옮기므로 표본이 살아 있는
 *       동안에만 유효하다.
 */
struct SegmentSummary {
    /// @brief 구간 이름이다. 로그에 그대로 나온다.
    const char* name{nullptr};

    /// @brief 이 구간 소요 시간의 중앙값(밀리초)이다.
    double medianMs{0.0};

    /// @brief 이 구간 소요 시간의 p95(밀리초)이다.
    double p95Ms{0.0};
};

/// @brief 한 요약에 담는 구간 요약 배열이다.
using FrameSegmentSummaries = std::array<SegmentSummary, core::diag::kMaxSamplesPerFrame>;

/**
 * @brief 링 버퍼에 모은 표본들의 요약이다.
 *
 * 타이밍 지표는 평균 대신 중앙값과 p95를 낸다. 평균은 한두 번 튀는 프레임에 끌려가
 * "이 설정의 비용"을 대표하지 못한다 (phase1.md §3). 중앙값은 짝수 표본에서 가운데
 * 두 값의 평균이고, p95는 nearest-rank로 ceil(0.95 × n)번째로 작은 값이다. 표본이
 * 20개 이하이면 p95는 최댓값과 같다.
 */
struct FrameSummary {
    /// @brief 요약에 포함된 프레임 수이다. 워밍업 표본은 세지 않는다. 0이면 표본이 없다.
    std::size_t frames{0};

    /// @brief cpu frame 중앙값(밀리초)이다.
    double cpuFrameMs{0.0};

    /// @brief cpu frame p95(밀리초)이다.
    double cpuFrameP95Ms{0.0};

    /// @brief gpu frame 중앙값(밀리초)이다. 읽은 표본이 없으면 0이다.
    double gpuFrameMs{0.0};

    /// @brief gpu frame p95(밀리초)이다. 읽은 표본이 없으면 0이다.
    double gpuFrameP95Ms{0.0};

    /// @brief fence wait 중앙값(밀리초)이다.
    double fenceWaitMs{0.0};

    /// @brief fence wait p95(밀리초)이다.
    double fenceWaitP95Ms{0.0};

    /// @brief 가장 최근 표본의 draw call 수이다. 창 안에서는 보통 일정하다.
    std::uint32_t drawCalls{0};

    /// @brief GPU 시간이 timestamp가 아니라 fence wait 근사치인지 여부이다.
    bool gpuApproximate{false};

    /// @brief 구간별 중앙값 · p95이다. 이름은 가장 최근 표본에서 가져온다.
    FrameSegmentSummaries segments{};

    /// @brief segments에 채워진 구간 수이다.
    std::size_t segmentCount{0};
};

/**
 * @brief 최근 프레임들의 지표를 링 버퍼에 모아 중앙값과 p95를 낸다.
 *
 * 매 프레임 로그를 찍지 않기 위한 버퍼다 (phase1.md §5.6). 엔진이 1초마다
 * summary()를 읽고 clear()한다.
 *
 * 앞 kWarmupFrames개 표본은 add()에서 버린다. 첫 프레임에는 파이프라인 워밍업과
 * 창 생성 비용이 섞여 있어 측정 대상이 아니다 (phase1.md §3). 워밍업 카운터는
 * clear()로 초기화하지 않는다 — 1초마다 clear해도 워밍업이 다시 시작되면 안 된다.
 *
 * @note 수명을 가진 객체다. FrameLoop가 소유한다.
 * @note Thread safety: thread-safe하지 않다. 프레임 루프 스레드 하나만 쓴다.
 */
class FrameStats final {
public:
    /// @brief 보관하는 표본 수이다. 60fps에서 약 4초에 해당한다.
    static constexpr std::size_t kCapacity = 240;

    /// @brief 측정에서 제외하는 Warm-up 프레임 수이다.
    static constexpr std::size_t kWarmupFrames = 60;

    /**
     * @brief 워밍업 프레임 수를 정한다.
     *
     * @param[in] warmupFrames 버릴 앞 표본 수. 테스트는 0이나 작은 값을 쓴다.
     */
    explicit FrameStats(std::size_t warmupFrames = kWarmupFrames) noexcept;

    /**
     * @brief 표본을 하나 더한다. 가득 차면 가장 오래된 것을 덮어쓴다.
     *
     * 워밍업이 남아 있으면 표본을 버리고 카운터만 줄인다.
     *
     * @param[in] sample 더할 표본.
     */
    void add(const FrameSample& sample) noexcept;

    /// @brief 지금까지 모은 표본의 중앙값과 p95를 반환한다.
    [[nodiscard]]
    FrameSummary summary() const noexcept;

    /**
     * @brief 표본을 모두 버린다. summary()를 호출한 직후에 부른다.
     *
     * 워밍업 카운터는 유지한다. 이 함수는 "이번 창을 비운다"는 뜻이지 "측정을
     * 처음부터 다시 시작한다"는 뜻이 아니다.
     */
    void clear() noexcept;

    /// @brief 지금 보관 중인 표본 수이다. 워밍업으로 버린 표본은 세지 않는다.
    [[nodiscard]]
    std::size_t count() const noexcept {
        return count_;
    }

    /// @brief 아직 워밍업 중인지 여부이다.
    [[nodiscard]]
    bool isWarmingUp() const noexcept {
        return (warmupRemaining_ > 0) || !measuring_;
    }

    /// @brief 남은 워밍업 표본 수이다. 0이면 측정 중이다.
    [[nodiscard]]
    std::size_t warmupRemaining() const noexcept {
        return warmupRemaining_;
    }

private:
    /// @brief 표본을 담는 링 버퍼이다.
    std::array<FrameSample, kCapacity> samples_{};

    /// @brief 버퍼에 들어 있는 표본 수이다. kCapacity를 넘지 않는다.
    std::size_t count_{0};

    /// @brief 다음 표본을 쓸 위치이다.
    std::size_t next_{0};

    /// @brief 아직 버릴 워밍업 표본 수이다.
    std::size_t warmupRemaining_{kWarmupFrames};

    /// @brief 첫 측정 표본이 들어온 이후인지 여부이다.
    bool measuring_{false};
};

} // namespace nxt::engine
