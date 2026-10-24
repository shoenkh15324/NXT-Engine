#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/frame/semaphore.hpp>

namespace nxt::graphics::rhi {

Semaphore createSemaphore(const Device& device) {
    // binary로 충분하다. RHI가 timeline semaphore를 노출하지 않는다 (CODING_RULES.md §10).
    vk::SemaphoreCreateInfo createInfo{};

    const auto created = vulkan::toVulkanDevice(device).device().createSemaphore(createInfo);
    VK_CHECK(created.result);

    const Semaphore semaphore = vulkan::fromVkHandle<SemaphoreTag>(created.value);
    NXT_LOG_DEBUG(Graphics, Vulkan, "semaphore created: semaphore={:#x}", semaphore.value());
    return semaphore;
}

void destroySemaphore(const Device& device, const Semaphore& semaphore) {
    NXT_ASSERT_MSG(semaphore.valid(), "유효하지 않은 semaphore를 해제하려 했다.");

    const vk::Semaphore vkSemaphore = vulkan::toVkHandle<vk::Semaphore>(semaphore);
    vulkan::toVulkanDevice(device).device().destroySemaphore(vkSemaphore);
    NXT_LOG_DEBUG(Graphics, Vulkan, "semaphore destroyed: semaphore={:#x}", semaphore.value());
}

} // namespace nxt::graphics::rhi
