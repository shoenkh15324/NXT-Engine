#include <array>
#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/frame/query.hpp>
#include <optional>
#include <vector>

namespace nxt::graphics::rhi {

TimestampQueryPool createTimestampQueryPool(const Device& device, std::uint32_t count) {
    NXT_ASSERT_MSG(count > 0, "timestamp query를 0개 만들려 했다.");

    const vulkan::VulkanDevice& vulkanDevice = vulkan::toVulkanDevice(device);

    // timestamp는 queue family 단위 기능이다. 지원하지 않는 family면 pool을 만들지
    // 않고 빈 값을 돌려준다 — 호출자는 fence wait 근사치로 대체한다 (phase1.md §5.1).
    const std::vector<vk::QueueFamilyProperties> families = vulkanDevice.physicalDevice().getQueueFamilyProperties();
    const std::uint32_t familyIndex = vulkanDevice.queueFamilyIndex();
    NXT_ASSERT_MSG(familyIndex < families.size(), "queue family 인덱스가 목록 범위를 벗어났다.");

    if (families[familyIndex].timestampValidBits == 0) {
        NXT_LOG_WARN(Graphics, Vulkan, "timestamp query unsupported: family={}", familyIndex);
        return {};
    }

    vk::QueryPoolCreateInfo createInfo{};
    createInfo.queryType = vk::QueryType::eTimestamp;
    createInfo.queryCount = count;

    const auto created = vulkanDevice.device().createQueryPool(createInfo);
    VK_CHECK(created.result);

    // tick을 나노초로 바꾸는 값은 디바이스가 준다. pool과 함께 들고 다녀야
    // 읽는 쪽이 나노초로 해석할 수 있다.
    const float timestampPeriod = vulkanDevice.physicalDevice().getProperties().limits.timestampPeriod;

    const TimestampQueryPool pool{
        .pool = vulkan::fromVkHandle<QueryPoolTag>(created.value),
        .count = count,
        .periodNs = static_cast<double>(timestampPeriod),
    };

    NXT_LOG_DEBUG(Graphics, Vulkan, "timestamp query pool created: count={} period={}ns", count, pool.periodNs);
    return pool;
}

void destroyTimestampQueryPool(const Device& device, const TimestampQueryPool& pool) {
    if (!pool.pool.valid()) {
        return;
    }

    const vk::QueryPool vkPool = vulkan::toVkHandle<vk::QueryPool>(pool.pool);
    vulkan::toVulkanDevice(device).device().destroyQueryPool(vkPool);
    NXT_LOG_DEBUG(Graphics, Vulkan, "timestamp query pool destroyed: pool={:#x}", pool.pool.value());
}

std::optional<std::uint64_t> readElapsedNs(const Device& device, const TimestampQueryPool& pool,
                                           std::uint32_t beginQuery, std::uint32_t endQuery) {
    NXT_ASSERT_MSG(pool.pool.valid(), "유효하지 않은 query pool에서 timestamp를 읽으려 했다.");
    NXT_ASSERT_MSG(endQuery == (beginQuery + 1), "timestamp 두 개를 연속된 인덱스로 읽어야 한다.");
    NXT_ASSERT_MSG(endQuery < pool.count, "query 인덱스가 pool 범위를 벗어났다.");

    std::array<std::uint64_t, 2> values{};

    // eWait여도 호출자는 fence를 이미 기다렸으므로 stall이 아니다. eNotReady가
    // 돌아오는 경우만 "아직 모른다"로 남기고, 그 밖의 실패는 GPU 오류로 종료한다.
    const vk::Result result = vulkan::toVulkanDevice(device).device().getQueryPoolResults(
        vulkan::toVkHandle<vk::QueryPool>(pool.pool), beginQuery, 2, sizeof(values), values.data(),
        sizeof(std::uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    if (result == vk::Result::eNotReady) {
        return std::nullopt;
    }
    VK_CHECK(result);

    // 부호 없는 뺄셈이라 카운터가 감싸도 차이는 맞다.
    return static_cast<std::uint64_t>(static_cast<double>(values[1] - values[0]) * pool.periodNs);
}

} // namespace nxt::graphics::rhi
