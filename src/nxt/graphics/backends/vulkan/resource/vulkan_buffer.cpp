#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/resource/buffer.hpp>

namespace nxt::graphics::rhi {

BufferAllocation createBuffer(const Device& device, const BufferDesc& desc) {
    NXT_ASSERT_MSG(desc.size > 0, "0바이트 버퍼는 만들 수 없다.");
    NXT_ASSERT_MSG(static_cast<std::uint32_t>(desc.usage) != 0, "용도가 없는 버퍼는 만들 수 없다.");

    const vk::Device vkDevice = vulkan::toVulkanDevice(device).device();

    vk::BufferCreateInfo createInfo{};
    createInfo.size = desc.size;
    createInfo.usage = vulkan::toVulkan(desc.usage);
    // Phase 0은 큐가 하나뿐이라 소유권 이전이 없다. exclusive면 충분하다 (rhi.md §8).
    createInfo.sharingMode = vk::SharingMode::eExclusive;

    const auto created = vkDevice.createBuffer(createInfo);
    VK_CHECK(created.result);

    // 정렬 때문에 요구 크기가 desc.size보다 클 수 있다. 할당은 requirements.size로
    // 하고, memory type도 이 버퍼가 쓸 수 있는 비트 안에서만 고른다.
    const vk::MemoryRequirements requirements = vkDevice.getBufferMemoryRequirements(created.value);
    const Memory memory = allocateMemory(device, requirements.size, desc.memoryProperty, requirements.memoryTypeBits);

    const vk::DeviceMemory vkMemory = vulkan::toVkHandle<vk::DeviceMemory>(memory);
    VK_CHECK(vkDevice.bindBufferMemory(created.value, vkMemory, 0));

    const BufferAllocation allocation{vulkan::fromVkHandle<rhi::BufferTag>(created.value), memory};
    NXT_LOG_DEBUG(Graphics, Vulkan, "buffer created: buffer={:#x} memory={:#x} size={} allocated={}",
                  allocation.buffer.value(), memory.value(), desc.size, requirements.size);
    return allocation;
}

void destroyBuffer(const Device& device, const BufferAllocation& allocation) {
    NXT_ASSERT_MSG(allocation.buffer.valid(), "유효하지 않은 buffer를 해제하려 했다.");
    NXT_ASSERT_MSG(allocation.memory.valid(), "유효하지 않은 memory를 해제하려 했다.");

    const vk::Device vkDevice = vulkan::toVulkanDevice(device).device();

    // 메모리를 먼저 해제하면 버퍼가 죽은 메모리를 가리킨다. 버퍼가 먼저다.
    const vk::Buffer vkBuffer = vulkan::toVkHandle<vk::Buffer>(allocation.buffer);
    vkDevice.destroyBuffer(vkBuffer);
    NXT_LOG_DEBUG(Graphics, Vulkan, "buffer destroyed: buffer={:#x}", allocation.buffer.value());

    freeMemory(device, allocation.memory);
}

} // namespace nxt::graphics::rhi
