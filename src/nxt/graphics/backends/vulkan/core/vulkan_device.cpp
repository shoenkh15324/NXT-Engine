#include <cstdint>
#include <cstring>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <string_view>
#include <vector>

namespace nxt::graphics::vulkan {

namespace {

/// @brief 고른 물리 디바이스와 그 안의 queue family이다.
struct DeviceSelection {
    vk::PhysicalDevice physicalDevice;
    std::uint32_t queueFamily;
};

/**
 * @brief surface에 present할 수 있는 그래픽스 queue family를 가진 첫 물리 디바이스를 고른다.
 *
 * 조건은 두 가지다 — 그 family에 그래픽스 큐가 있고(queueFlags), 그 큐가 이 surface에
 * present할 수 있다(getSurfaceSupportKHR). 둘 중 하나만 보면 논리 디바이스 생성이나
 * 나중의 present에서 실패한다. 물리 디바이스 선택이 Surface에 달려 있는 이유다
 * (rhi.md §8).
 *
 * @param[in] instance 물리 디바이스를 열거할 인스턴스.
 * @param[in] surface  present 지원 여부를 확인할 surface.
 * @return 고른 디바이스와 queue family. 조건을 만족하는 것이 없으면 종료한다.
 */
[[nodiscard]]
DeviceSelection selectDevice(vk::Instance instance, vk::SurfaceKHR surface) {
    const auto enumerated = instance.enumeratePhysicalDevices();
    VK_CHECK(enumerated.result);
    const std::vector<vk::PhysicalDevice>& devices = enumerated.value;

    if (devices.empty()) {
        ::nxt::core::diag::reportAssertFailure(
            {"physical devices", "Vulkan을 지원하는 GPU가 없다.", __FILE__, __func__, __LINE__});
    }

    for (const vk::PhysicalDevice device : devices) {
        const std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

        for (std::uint32_t index = 0; index < families.size(); ++index) {
            if (!(families[index].queueFlags & vk::QueueFlagBits::eGraphics)) {
                continue;
            }

            const auto support = device.getSurfaceSupportKHR(index, surface);
            VK_CHECK(support.result);
            if (!support.value) {
                continue;
            }

            // 무엇을 골랐는지 남긴다. 다중 GPU 환경에서 "왜 이 GPU인가"를 추적할 수
            // 있어야 한다. Phase 0은 삼각형 하나라 우선순위 규칙을 두지 않는다 —
            // 조건을 만족하는 첫 디바이스를 쓴다 (CODING_RULES.md §10).
            const vk::PhysicalDeviceProperties properties = device.getProperties();
            NXT_LOG_INFO(Graphics, Vulkan, "physical device selected: {} ({}) family={}",
                         std::string_view(properties.deviceName), vk::to_string(properties.deviceType), index);
            return {device, index};
        }
    }

    ::nxt::core::diag::reportAssertFailure(
        {"queue family", "graphics와 present를 함께 지원하는 queue family가 없다.", __FILE__, __func__, __LINE__});
}

/**
 * @brief 물리 디바이스가 디바이스 확장을 지원하는지 확인한다.
 *
 * 인스턴스 확장(VK_KHR_surface)과 디바이스 확장(VK_KHR_swapchain)은 다른 목록이다.
 * 지원하지 않는 확장을 켜면 vkCreateDevice가 VK_ERROR_EXTENSION_NOT_PRESENT로
 * 실패하는데, 그 시점에는 어느 확장이 문제인지 알 수 없다. 그래서 먼저 확인해
 * 이름을 남긴다 — GPU 오류는 복구 불가능하므로 로그가 유일한 단서다 (rhi.md §7).
 *
 * @param[in] physicalDevice 확인할 디바이스.
 * @param[in] name           필요한 확장 이름.
 */
void requireDeviceExtension(vk::PhysicalDevice physicalDevice, const char* name) {
    const auto enumerated = physicalDevice.enumerateDeviceExtensionProperties();
    VK_CHECK(enumerated.result);

    for (const vk::ExtensionProperties& extension : enumerated.value) {
        if (std::strcmp(extension.extensionName.data(), name) == 0) {
            return;
        }
    }

    NXT_LOG_ERROR(Graphics, Vulkan, "device extension not supported: {}", std::string_view{name});
    ::nxt::core::diag::reportAssertFailure(
        {"device extension", "필요한 device 확장을 지원하지 않는 GPU이다.", __FILE__, __func__, __LINE__});
}

} // namespace

std::unique_ptr<VulkanDevice> VulkanDevice::create(vk::Instance instance, vk::SurfaceKHR surface) {
    const DeviceSelection selection = selectDevice(instance, surface);

    const float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo{};
    queueInfo.queueFamilyIndex = selection.queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    // VK_KHR_swapchain은 디바이스 확장이다. A5에서 swapchain을 만들기 시작하므로
    // 여기서 켠다 (rhi.md §8). 확장이 없으면 vkCreateDevice가 실패하는데 그때는
    // 원인을 알 수 없으므로 먼저 지원 여부를 확인한다.
    requireDeviceExtension(selection.physicalDevice, vk::KHRSwapchainExtensionName);

    const char* const enabledExtensions[] = {vk::KHRSwapchainExtensionName};

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = enabledExtensions;

    const auto created = selection.physicalDevice.createDevice(createInfo);
    VK_CHECK(created.result);
    const vk::Device device = created.value;

    // queue family를 하나만 쓰므로 graphics와 present가 같은 큐다 (rhi.md §8).
    const vk::Queue graphicsQueue = device.getQueue(selection.queueFamily, 0);
    const vk::Queue presentQueue = device.getQueue(selection.queueFamily, 0);

    // 커맨드 버퍼는 pool에서 꺼낸다. 스레드도 큐도 하나라 pool을 나눌 이유가 없다
    // (rhi.md §8). eResetCommandBuffer는 매 프레임 같은 버퍼를 다시 기록하기 위한
    // 것이다 (frame.md §5).
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = selection.queueFamily;

    const auto pool = device.createCommandPool(poolInfo);
    VK_CHECK(pool.result);

    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다.
    return std::unique_ptr<VulkanDevice>(
        new VulkanDevice(selection.physicalDevice, device, pool.value, graphicsQueue, presentQueue));
}

VulkanDevice::VulkanDevice(vk::PhysicalDevice physicalDevice, vk::Device device, vk::CommandPool commandPool,
                           vk::Queue graphicsQueue, vk::Queue presentQueue) noexcept
    : physicalDevice_{physicalDevice},
      device_{device},
      graphicsQueue_{graphicsQueue},
      presentQueue_{presentQueue},
      commandPool_{commandPool} {}

VulkanDevice::~VulkanDevice() {
    // queue는 디바이스가 소유하므로 함께 사라진다. pool은 디바이스보다 먼저 파괴한다.
    device_.destroyCommandPool(commandPool_);
    device_.destroy();
}

rhi::Queue VulkanDevice::graphicsQueue() const noexcept {
    return fromVkHandle<rhi::QueueTag>(graphicsQueue_);
}

rhi::Queue VulkanDevice::presentQueue() const noexcept {
    return fromVkHandle<rhi::QueueTag>(presentQueue_);
}

const VulkanDevice& toVulkanDevice(const rhi::Device& device) noexcept {
    NXT_ASSERT_MSG(dynamic_cast<const VulkanDevice*>(&device) != nullptr, "Vulkan 백엔드가 아닌 Device를 넘겼다.");
    return static_cast<const VulkanDevice&>(device);
}

} // namespace nxt::graphics::vulkan
