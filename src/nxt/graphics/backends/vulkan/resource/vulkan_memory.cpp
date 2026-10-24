#include <cstddef>
#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/resource/memory.hpp>

namespace nxt::graphics::vulkan {

namespace {

/**
 * @brief 요구한 성질을 모두 가진 memory type index를 찾는다.
 *
 * 후보는 typeBits에서 켜진 비트뿐이고, 그중 propertyFlags가 required를 모두
 * 포함하는 첫 타입을 고른다. 둘 다 봐야 한다 — 성질만 보면 이 리소스에 바인딩할
 * 수 없는 타입을 고르고, 비트만 보면 HostVisible이 아닌 타입을 골라 map이 실패한다.
 *
 * 첫 매치를 쓴다. spec이 memory type 배열의 순서를 정하지는 않지만, 드라이버는
 * 보통 VRAM(DeviceLocal 전용)을 낮은 index에 둔다. 그래서 요구 성질을 만족하는
 * 것 중 앞쪽이 실용적으로 더 빠른 타입이다. 우선순위 규칙이 필요해지면 측정 뒤에
 * 넣는다 (CODING_RULES.md §10).
 *
 * Vulkan은 memoryTypeCount를 최대 32로 제한하므로 1u << index가 안전하다.
 *
 * @param[in] physicalDevice memory type을 조회할 디바이스.
 * @param[in] typeBits       후보 memory type 비트 집합.
 * @param[in] required       필요한 메모리 성질.
 * @return 고른 memory type index. 없으면 종료한다.
 */
[[nodiscard]]
std::uint32_t findMemoryTypeIndex(vk::PhysicalDevice physicalDevice, std::uint32_t typeBits,
                                  rhi::MemoryProperty required) {
    const vk::PhysicalDeviceMemoryProperties properties = physicalDevice.getMemoryProperties();
    const vk::MemoryPropertyFlags requiredFlags = toVulkan(required);

    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        const bool allowed = (typeBits & (1u << index)) != 0;
        const vk::MemoryPropertyFlags flags = properties.memoryTypes[index].propertyFlags;
        if (allowed && ((flags & requiredFlags) == requiredFlags)) {
            return index;
        }
    }

    ::nxt::core::diag::reportAssertFailure(
        {"memory type", "요구한 성질을 모두 가진 memory type이 없다.", __FILE__, __func__, __LINE__});
}

} // namespace

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

Memory allocateMemory(const Device& device, std::uint64_t size, MemoryProperty required) {
    NXT_ASSERT_MSG(size > 0, "0바이트 메모리는 할당할 수 없다.");

    const vulkan::VulkanDevice& backend = vulkan::toVulkanDevice(device);

    // 아직 버퍼·이미지가 없으므로 모든 memory type을 후보로 둔다. B4에서
    // vkGetBufferMemoryRequirements가 준 typeBits를 넘기는 경로가 생긴다.
    constexpr std::uint32_t kAllMemoryTypes = 0xFFFFFFFFU;
    const std::uint32_t typeIndex = vulkan::findMemoryTypeIndex(backend.physicalDevice(), kAllMemoryTypes, required);

    vk::MemoryAllocateInfo allocateInfo{};
    allocateInfo.allocationSize = size;
    allocateInfo.memoryTypeIndex = typeIndex;

    const auto allocated = backend.device().allocateMemory(allocateInfo);
    VK_CHECK(allocated.result);

    const Memory memory = vulkan::fromVkHandle<MemoryTag>(allocated.value);
    NXT_LOG_DEBUG(Graphics, Vulkan, "memory allocated: memory={:#x} size={} type={}", memory.value(), size, typeIndex);
    return memory;
}

void freeMemory(const Device& device, const Memory& memory) {
    NXT_ASSERT_MSG(memory.valid(), "유효하지 않은 memory를 해제하려 했다.");

    const vk::DeviceMemory vkMemory = vulkan::toVkHandle<vk::DeviceMemory>(memory);
    vulkan::toVulkanDevice(device).device().freeMemory(vkMemory);
    NXT_LOG_DEBUG(Graphics, Vulkan, "memory freed: memory={:#x}", memory.value());
}

void* mapMemory(const Device& device, const Memory& memory, std::uint64_t offset, std::size_t size) {
    NXT_ASSERT_MSG(memory.valid(), "유효하지 않은 memory를 매핑하려 했다.");

    const vk::DeviceMemory vkMemory = vulkan::toVkHandle<vk::DeviceMemory>(memory);
    const auto mapped = vulkan::toVulkanDevice(device).device().mapMemory(vkMemory, offset, size);
    VK_CHECK(mapped.result);

    NXT_LOG_DEBUG(Graphics, Vulkan, "memory mapped: memory={:#x} offset={} size={}", memory.value(), offset, size);
    return mapped.value;
}

void unmapMemory(const Device& device, const Memory& memory) {
    NXT_ASSERT_MSG(memory.valid(), "유효하지 않은 memory의 매핑을 해제하려 했다.");

    const vk::DeviceMemory vkMemory = vulkan::toVkHandle<vk::DeviceMemory>(memory);
    vulkan::toVulkanDevice(device).device().unmapMemory(vkMemory);
    NXT_LOG_DEBUG(Graphics, Vulkan, "memory unmapped: memory={:#x}", memory.value());
}

} // namespace nxt::graphics::rhi
