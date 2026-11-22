#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <nxt/core/time/time.hpp>
#include <nxt/engine/frame_stats.hpp>

namespace nxt::engine {

namespace {

/**
 * @brief 나노초를 밀리초로 바꾼다.
 * @param[in] ns 바꿀 나노초.
 * @return 밀리초 값.
 */
[[nodiscard]]
constexpr double toMs(std::uint64_t ns) noexcept {
    return static_cast<double>(ns) / static_cast<double>(core::time::kNsPerMs);
}

/**
 * @brief 한 지표의 표본을 모으고 정렬하는 창이다.
 *
 * FrameStats::kCapacity개까지만 담는다. summary()가 링 버퍼를 한 번 훑을 때 지표마다
 * 하나씩 만들고, 채워진 부분만 정렬해 percentile을 읽는다.
 */
struct WindowValues {
    /// @brief 정렬 전 표본이다. 값이 없는 칸은 읽지 않는다.
    std::array<std::uint64_t, FrameStats::kCapacity> values{};

    /// @brief values에 채워진 표본 수이다.
    std::size_t count{0};

    /// @brief 표본을 하나 더한다. 호출자는 count < kCapacity를 보장한다.
    void add(std::uint64_t value) noexcept {
        values[count] = value;
        ++count;
    }

    /// @brief 채워진 부분만 오름차순으로 정렬한다.
    void sort() noexcept {
        std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
    }
};

/**
 * @brief 중앙값을 밀리초로 반환한다.
 *
 * 짝수 표본은 가운데 두 값의 평균이다. 표본이 없으면 0이다.
 *
 * @param[in] window 정렬된 표본 창.
 * @return 중앙값(밀리초).
 */
[[nodiscard]]
double medianMs(const WindowValues& window) noexcept {
    if (window.count == 0) {
        return 0.0;
    }

    const std::size_t middle = window.count / 2;
    if ((window.count % 2) != 0) {
        return toMs(window.values[middle]);
    }
    return toMs((window.values[middle - 1] + window.values[middle]) / 2);
}

/**
 * @brief p95를 밀리초로 반환한다.
 *
 * nearest-rank 방법으로 ceil(0.95 × n)번째로 작은 값을 쓴다. 표본이 20개 이하이면
 * 이 순위가 마지막 표본이라 p95는 최댓값이 된다. 표본이 없으면 0이다.
 *
 * @param[in] window 정렬된 표본 창.
 * @return p95(밀리초).
 */
[[nodiscard]]
double p95Ms(const WindowValues& window) noexcept {
    if (window.count == 0) {
        return 0.0;
    }

    // (count * 95 + 99) / 100 은 ceil(count * 0.95)와 같다. count는 kCapacity 이하라
    // 곱해도 넘치지 않는다.
    const std::size_t rank = ((window.count * 95) + 99) / 100;
    return toMs(window.values[std::min(rank, window.count) - 1]);
}

} // namespace

FrameStats::FrameStats(std::size_t warmupFrames) noexcept : warmupRemaining_(warmupFrames) {}

void FrameStats::add(const FrameSample& sample) noexcept {
    // 워밍업 표본은 버린다. 카운터는 clear()로 초기화하지 않으므로 1초 summary가
    // 비어있어도 워밍업이 다시 시작되지 않는다.
    if (warmupRemaining_ > 0) {
        --warmupRemaining_;
        return;
    }

    measuring_ = true;
    samples_[next_] = sample;
    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) {
        ++count_;
    }
}

FrameSummary FrameStats::summary() const noexcept {
    FrameSummary result{};
    if (count_ == 0) {
        return result;
    }

    WindowValues cpu;
    WindowValues fence;
    WindowValues gpu;
    std::array<WindowValues, core::diag::kMaxSamplesPerFrame> segmentValues{};

    for (std::size_t index = 0; index < count_; ++index) {
        const FrameSample& sample = samples_[index];

        cpu.add(sample.cpuFrameNs);
        fence.add(sample.fenceWaitNs);

        if (sample.gpuApproximate) {
            result.gpuApproximate = true;
        }
        if (sample.gpuFrameNs > 0) {
            gpu.add(sample.gpuFrameNs);
        }

        for (std::size_t slot = 0; (slot < sample.segmentCount) && (slot < segmentValues.size()); ++slot) {
            segmentValues[slot].add(sample.segments[slot].ns);
        }
    }

    cpu.sort();
    fence.sort();
    gpu.sort();

    const std::size_t newestIndex = (next_ + kCapacity - 1) % kCapacity;
    const FrameSample& newest = samples_[newestIndex];

    result.frames = count_;
    result.cpuFrameMs = medianMs(cpu);
    result.cpuFrameP95Ms = p95Ms(cpu);
    result.fenceWaitMs = medianMs(fence);
    result.fenceWaitP95Ms = p95Ms(fence);
    result.gpuFrameMs = medianMs(gpu);
    result.gpuFrameP95Ms = p95Ms(gpu);
    result.drawCalls = newest.drawCalls;

    // 구간 순서는 빌드마다 고정이다. 이름은 가장 최근 표본에서 가져와 통계에 붙인다.
    // 최근 표본에 없는 구간은 이름을 붙일 수 없으므로 거기서 멈춘다.
    for (std::size_t slot = 0; (slot < newest.segmentCount) && (slot < segmentValues.size()); ++slot) {
        WindowValues& window = segmentValues[slot];
        if (window.count == 0) {
            break;
        }

        window.sort();
        result.segments[slot] = SegmentSummary{
            .name = newest.segments[slot].name,
            .medianMs = medianMs(window),
            .p95Ms = p95Ms(window),
        };
        ++result.segmentCount;
    }

    return result;
}

void FrameStats::clear() noexcept {
    count_ = 0;
    next_ = 0;
}

} // namespace nxt::engine
