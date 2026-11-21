#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <optional>

namespace nxt::graphics::rhi {

class Device;

/// @brief QueryPool을 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct QueryPoolTag {};

/**
 * @brief GPU가 기록한 값을 나중에 꺼내는 질의 객체이다.
 *
 * Phase 1이 쓰는 것은 timestamp 하나뿐이다. occlusion query 같은 일반화는 측정이
 * 요구할 때 추가한다 (phase1.md §5.1).
 *
 * @note 값 핸들이다. 만들고 파괴하는 것은 createTimestampQueryPool()과
 *       destroyTimestampQueryPool()이다.
 * @note 소유권이 없다. GPU가 아직 쓰고 있을 수 있으므로 fence 신호 뒤에 파괴한다.
 */
using QueryPool = ResourceHandle<QueryPoolTag>;

/**
 * @brief timestamp query pool과 그 변환 주기이다.
 *
 * GPU timestamp는 불투명한 tick 값이다. 나노초로 바꾸려면 디바이스가 주는
 * timestampPeriod(tick당 나노초)가 필요하므로, pool과 함께 들고 다닌다
 * (phase1.md §5.1).
 *
 * @note 미지원 GPU에서는 pool이 유효하지 않다. 호출자는 pool.valid()를
 *       "GPU 시간을 잴 수 있는가"로 읽는다.
 */
struct TimestampQueryPool {
    /// @brief 만들어지지 않았거나 미지원이면 유효하지 않은 핸들이다.
    QueryPool pool{};

    /// @brief pool에 들어 있는 query 수이다.
    std::uint32_t count{0};

    /// @brief tick 하나가 몇 나노초인지. 디바이스의 limits.timestampPeriod이다.
    double periodNs{0.0};
};

/**
 * @brief timestamp query pool을 만든다.
 *
 * queue family가 timestamp를 지원하지 않으면 pool이 유효하지 않은 값을 돌려주고
 * 경고를 남긴다. 실패를 예외로 만들지 않는 것은 rhi의 다른 create와 같다.
 *
 * @param[in] device pool을 만들 디바이스.
 * @param[in] count  pool에 넣을 timestamp 수. 0이면 안 된다.
 * @return 만들어진 pool과 변환 주기. 미지원이면 pool.valid()가 false다.
 */
[[nodiscard]]
TimestampQueryPool createTimestampQueryPool(const Device& device, std::uint32_t count);

/**
 * @brief timestamp query pool을 해제한다.
 *
 * 그 pool을 쓰는 command buffer가 GPU에서 끝난 뒤에 부른다. 프레임 레인과 함께
 * 파괴하는 것이 보통이다.
 *
 * @param[in] device pool을 만든 디바이스.
 * @param[in] pool   해제할 pool. 유효하지 않으면 아무 일도 하지 않는다.
 */
void destroyTimestampQueryPool(const Device& device, const TimestampQueryPool& pool);

/**
 * @brief 두 timestamp의 차이를 나노초로 읽는다.
 *
 * 그 command buffer의 fence가 신호된 뒤에만 부른다. 결과는 eWait로 기다려 읽으므로
 * 이미 준비된 값을 꺼내는 것이고 GPU를 stall시키지 않는다 (phase1.md §5.1).
 *
 * @param[in] device     pool을 만든 디바이스.
 * @param[in] pool       읽을 pool.
 * @param[in] beginQuery 시작 timestamp의 query 인덱스.
 * @param[in] endQuery   끝 timestamp의 query 인덱스. beginQuery + 1이어야 한다.
 * @return 두 timestamp의 차이(나노초). 아직 준비되지 않았으면 nullopt.
 *
 * @note GPU timestamp 카운터가 감싸도 부호 없는 뺄셈이라 차이는 맞는다. 유효
 *       비트가 64보다 작은 GPU도 프레임 단위 차이에는 영향이 없다.
 */
[[nodiscard]]
std::optional<std::uint64_t> readElapsedNs(const Device& device, const TimestampQueryPool& pool,
                                           std::uint32_t beginQuery, std::uint32_t endQuery);

} // namespace nxt::graphics::rhi
