#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/engine/frame_stats.hpp>
#include <string_view>

namespace {

namespace diag = nxt::core::diag;
namespace engine = nxt::engine;

/// @brief 1밀리초를 나노초로 나타낸 값이다.
constexpr std::uint64_t kMs = 1'000'000;

/**
 * @brief cpu · fence 시간만 채운 표본을 만든다.
 *
 * @param[in] cpuMs   cpu frame 시간(밀리초).
 * @param[in] fenceMs fence wait 시간(밀리초).
 * @return 채운 표본.
 */
[[nodiscard]]
engine::FrameSample makeSample(std::uint64_t cpuMs, std::uint64_t fenceMs = 0) noexcept {
    return engine::FrameSample{
        .cpuFrameNs = cpuMs * kMs,
        .fenceWaitNs = fenceMs * kMs,
    };
}

} // namespace

TEST_CASE("워밍업 표본은 통계에서 제외된다.") {
    engine::FrameStats stats{3};

    for (std::uint32_t index = 0; index < 3; ++index) {
        stats.add(makeSample(100));
    }

    CHECK(stats.isWarmingUp());
    CHECK(stats.count() == 0);
    CHECK(stats.summary().frames == 0);

    stats.add(makeSample(2));
    stats.add(makeSample(4));

    CHECK_FALSE(stats.isWarmingUp());
    CHECK(stats.count() == 2);

    const engine::FrameSummary summary = stats.summary();
    CHECK(summary.frames == 2);
    CHECK(summary.cpuFrameMs == doctest::Approx(3.0)); // (2 + 4) / 2
    CHECK(summary.cpuFrameP95Ms == doctest::Approx(4.0));
}

TEST_CASE("clear()는 표본을 비우고 워밍업 카운터는 유지한다.") {
    engine::FrameStats stats{2};

    stats.add(makeSample(10)); // 워밍업 1/2
    stats.clear();

    CHECK(stats.isWarmingUp());
    CHECK(stats.count() == 0);

    stats.add(makeSample(10)); // 워밍업 2/2
    CHECK(stats.isWarmingUp());

    stats.add(makeSample(10)); // 측정 시작
    CHECK_FALSE(stats.isWarmingUp());
    CHECK(stats.count() == 1);
    CHECK(stats.summary().frames == 1);
}

TEST_CASE("중앙값은 홀수 표본에서 가운데 값이다.") {
    engine::FrameStats stats{0};

    stats.add(makeSample(3));
    stats.add(makeSample(1));
    stats.add(makeSample(2));

    const engine::FrameSummary summary = stats.summary();
    CHECK(summary.cpuFrameMs == doctest::Approx(2.0));
    CHECK(summary.cpuFrameP95Ms == doctest::Approx(3.0)); // n=3이면 p95는 최댓값
}

TEST_CASE("중앙값은 짝수 표본에서 가운데 두 값의 평균이다.") {
    engine::FrameStats stats{0};

    for (const std::uint64_t value : {4ull, 1ull, 3ull, 2ull}) {
        stats.add(makeSample(value));
    }

    CHECK(stats.summary().cpuFrameMs == doctest::Approx(2.5));
}

TEST_CASE("p95는 nearest-rank로 ceil(0.95 * n)번째 값이다.") {
    SUBCASE("표본 20개") {
        engine::FrameStats stats{0};
        for (std::uint64_t value = 1; value <= 20; ++value) {
            stats.add(makeSample(value));
        }

        // ceil(0.95 * 20) = 19번째 값이다.
        CHECK(stats.summary().cpuFrameP95Ms == doctest::Approx(19.0));
    }

    SUBCASE("표본 100개") {
        engine::FrameStats stats{0};
        for (std::uint64_t value = 1; value <= 100; ++value) {
            stats.add(makeSample(value));
        }

        CHECK(stats.summary().cpuFrameP95Ms == doctest::Approx(95.0));
    }

    SUBCASE("표본 1개") {
        engine::FrameStats stats{0};
        stats.add(makeSample(7));
        CHECK(stats.summary().cpuFrameP95Ms == doctest::Approx(7.0));
    }
}

TEST_CASE("GPU 표본이 없는 프레임은 GPU 통계에서 빠진다.") {
    engine::FrameStats stats{0};

    engine::FrameSample noGpu = makeSample(1);
    engine::FrameSample withGpu = makeSample(1);
    withGpu.gpuFrameNs = 2 * kMs;
    stats.add(noGpu);
    stats.add(withGpu);

    const engine::FrameSummary summary = stats.summary();
    CHECK(summary.gpuFrameMs == doctest::Approx(2.0));
    CHECK(summary.gpuFrameP95Ms == doctest::Approx(2.0));
    CHECK_FALSE(summary.gpuApproximate);
}

TEST_CASE("GPU 근사치 표본은 gpuApproximate를 켠다.") {
    engine::FrameStats stats{0};

    engine::FrameSample sample = makeSample(1);
    sample.gpuFrameNs = 1 * kMs;
    sample.gpuApproximate = true;
    stats.add(sample);

    CHECK(stats.summary().gpuApproximate);
}

TEST_CASE("draw call 수는 가장 최근 표본을 쓴다.") {
    engine::FrameStats stats{0};

    engine::FrameSample first = makeSample(1);
    first.drawCalls = 10;
    engine::FrameSample last = makeSample(1);
    last.drawCalls = 42;
    stats.add(first);
    stats.add(last);

    CHECK(stats.summary().drawCalls == 42);
}

TEST_CASE("구간은 이름과 함께 중앙값 · p95를 낸다.") {
    engine::FrameStats stats{0};

    for (std::uint64_t value = 1; value <= 3; ++value) {
        engine::FrameSample sample = makeSample(1);
        sample.segments[0] = diag::Sample{.name = "record", .ns = value * kMs};
        sample.segments[1] = diag::Sample{.name = "submit", .ns = (value + 10) * kMs};
        sample.segmentCount = 2;
        stats.add(sample);
    }

    const engine::FrameSummary summary = stats.summary();
    REQUIRE(summary.segmentCount == 2);
    CHECK(std::string_view{summary.segments[0].name} == "record");
    CHECK(summary.segments[0].medianMs == doctest::Approx(2.0));
    CHECK(summary.segments[0].p95Ms == doctest::Approx(3.0));
    CHECK(summary.segments[1].medianMs == doctest::Approx(12.0));
    CHECK(summary.segments[1].p95Ms == doctest::Approx(13.0));
}
