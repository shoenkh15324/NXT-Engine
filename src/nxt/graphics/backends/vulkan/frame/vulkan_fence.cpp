#include <cstdint>
#include <limits>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/frame/fence.hpp>

namespace nxt::graphics::rhi {

Fence createFence(const Device& device) {
    // 제출 전에 대기하지 않도록 신호된 상태로 만든다 (fence.hpp).
    vk::FenceCreateInfo createInfo{};
    createInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    const auto created = vulkan::toVulkanDevice(device).device().createFence(createInfo);
    VK_CHECK(created.result);

    const Fence fence = vulkan::fromVkHandle<FenceTag>(created.value);
    NXT_LOG_DEBUG(Graphics, Vulkan, "fence created: fence={:#x}", fence.value());
    return fence;
}

void destroyFence(const Device& device, const Fence& fence) {
    NXT_ASSERT_MSG(fence.valid(), "유효하지 않은 fence를 해제하려 했다.");

    const vk::Fence vkFence = vulkan::toVkHandle<vk::Fence>(fence);
    vulkan::toVulkanDevice(device).device().destroyFence(vkFence);
    NXT_LOG_DEBUG(Graphics, Vulkan, "fence destroyed: fence={:#x}", fence.value());
}

void waitFence(const Device& device, const Fence& fence) {
    NXT_ASSERT_MSG(fence.valid(), "유효하지 않은 fence를 기다리려 했다.");

    // timeout 인자를 두지 않는다 — 무한정 기다리는 쪽이 옳다고 가정한다 (frame.md §5).
    constexpr std::uint64_t kWaitForever = std::numeric_limits<std::uint64_t>::max();
    const vk::Fence vkFence = vulkan::toVkHandle<vk::Fence>(fence);
    VK_CHECK(vulkan::toVulkanDevice(device).device().waitForFences(vkFence, vk::True, kWaitForever));
}

void resetFence(const Device& device, const Fence& fence) {
    NXT_ASSERT_MSG(fence.valid(), "유효하지 않은 fence를 되돌리려 했다.");

    const vk::Fence vkFence = vulkan::toVkHandle<vk::Fence>(fence);
    VK_CHECK(vulkan::toVulkanDevice(device).device().resetFences(vkFence));
}

bool isSignaled(const Device& device, const Fence& fence) {
    NXT_ASSERT_MSG(fence.valid(), "유효하지 않은 fence의 상태를 확인하려 했다.");

    const vk::Fence vkFence = vulkan::toVkHandle<vk::Fence>(fence);
    const vk::Result status = vulkan::toVulkanDevice(device).device().getFenceStatus(vkFence);

    // eNotReady는 "아직 신호되지 않았다"는 정상 반환이다. VK_CHECK는 eSuccess가
    // 아니면 종료하므로 그 앞에서 걸러야 한다.
    if (status == vk::Result::eNotReady) {
        return false;
    }

    VK_CHECK(status);
    return true;
}

} // namespace nxt::graphics::rhi
