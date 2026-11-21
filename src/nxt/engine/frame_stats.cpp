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

} // namespace

void FrameStats::add(const FrameSample& sample) noexcept {
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

    std::uint64_t cpuSum = 0;
    std::uint64_t fenceSum = 0;
    std::uint64_t drawSum = 0;
    std::uint64_t gpuSum = 0;
    std::size_t gpuSamples = 0;
    std::array<std::uint64_t, core::diag::kMaxSamplesPerFrame> segmentSum{};
    std::size_t segmentCount = 0;

    for (std::size_t index = 0; index < count_; ++index) {
        const FrameSample& sample = samples_[index];

        cpuSum += sample.cpuFrameNs;
        fenceSum += sample.fenceWaitNs;
        drawSum += sample.drawCalls;

        if (sample.gpuApproximate) {
            result.gpuApproximate = true;
        }
        if (sample.gpuFrameNs > 0) {
            gpuSum += sample.gpuFrameNs;
            ++gpuSamples;
        }

        segmentCount = std::max(segmentCount, sample.segmentCount);
        for (std::size_t slot = 0; slot < sample.segmentCount; ++slot) {
            segmentSum[slot] += sample.segments[slot].ns;
        }
    }

    const std::size_t newestIndex = (next_ + kCapacity - 1) % kCapacity;
    const FrameSample& newest = samples_[newestIndex];

    result.frames = count_;
    result.cpuFrameMs = toMs(cpuSum / count_);
    result.fenceWaitMs = toMs(fenceSum / count_);
    result.drawCalls = static_cast<std::uint32_t>(drawSum / count_);
    if (gpuSamples > 0) {
        result.gpuFrameMs = toMs(gpuSum / gpuSamples);
    }

    // 구간 순서는 빌드마다 고정이다. 이름은 가장 최근 표본에서 가져와 평균값에
    // 붙인다. 최근 표본에 없는 구간은 이름을 붙일 수 없으므로 거기서 멈춘다.
    for (std::size_t slot = 0; (slot < segmentCount) && (slot < newest.segmentCount); ++slot) {
        result.segments[slot].name = newest.segments[slot].name;
        result.segments[slot].ns = segmentSum[slot] / count_;
        ++result.segmentCount;
    }

    return result;
}

void FrameStats::clear() noexcept {
    count_ = 0;
    next_ = 0;
}

} // namespace nxt::engine
