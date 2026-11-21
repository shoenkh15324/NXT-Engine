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
 * @brief 링 버퍼에 모은 표본들의 요약이다.
 *
 * 중앙값·p95는 1-D2가 링 버퍼 위에 얹는다. 지금은 평균만 낸다 — 1-A2의 판정은
 * "CPU와 GPU가 분리 측정되는가"까지다.
 */
struct FrameSummary {
    /// @brief 요약에 포함된 프레임 수이다. 0이면 아직 표본이 없다.
    std::size_t frames{0};

    /// @brief cpu frame 평균(밀리초)이다.
    double cpuFrameMs{0.0};

    /// @brief gpu frame 평균(밀리초)이다. 읽은 표본이 없으면 0이다.
    double gpuFrameMs{0.0};

    /// @brief fence wait 평균(밀리초)이다.
    double fenceWaitMs{0.0};

    /// @brief 프레임당 draw call 평균이다.
    std::uint32_t drawCalls{0};

    /// @brief GPU 시간이 timestamp가 아니라 fence wait 근사치인지 여부이다.
    bool gpuApproximate{false};

    /// @brief 구간별 평균이다. 이름은 가장 최근 표본에서 가져온다.
    core::diag::FrameSamples segments{};

    /// @brief segments에 채워진 구간 수이다.
    std::size_t segmentCount{0};
};

/**
 * @brief 최근 프레임들의 지표를 링 버퍼에 모으고 평균을 낸다.
 *
 * 매 프레임 로그를 찍지 않기 위한 버퍼다 (phase1.md §5.6). 엔진이 1초마다
 * summary()를 읽고 clear()한다.
 *
 * @note 수명을 가진 객체다. FrameLoop가 소유한다.
 * @note Thread safety: thread-safe하지 않다. 프레임 루프 스레드 하나만 쓴다.
 */
class FrameStats final {
public:
    /// @brief 보관하는 표본 수이다. 60fps에서 약 4초에 해당한다.
    static constexpr std::size_t kCapacity = 240;

    /**
     * @brief 표본을 하나 더한다. 가득 차면 가장 오래된 것을 덮어쓴다.
     *
     * @param[in] sample 더할 표본.
     */
    void add(const FrameSample& sample) noexcept;

    /// @brief 지금까지 모은 표본의 평균을 반환한다.
    [[nodiscard]]
    FrameSummary summary() const noexcept;

    /// @brief 표본을 모두 버린다. summary()를 호출한 직후에 부른다.
    void clear() noexcept;

    /// @brief 지금 보관 중인 표본 수를 반환한다.
    [[nodiscard]]
    std::size_t count() const noexcept {
        return count_;
    }

private:
    /// @brief 표본을 담는 링 버퍼이다.
    std::array<FrameSample, kCapacity> samples_{};

    /// @brief 버퍼에 들어 있는 표본 수이다. kCapacity를 넘지 않는다.
    std::size_t count_{0};

    /// @brief 다음 표본을 쓸 위치이다.
    std::size_t next_{0};
};

} // namespace nxt::engine
