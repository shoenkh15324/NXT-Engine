#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_instance.hpp>
#include <nxt/platform/window/window.hpp>
#include <string>
#include <vector>

// 플랫폼 헤더를 여기서 알지 않는다. surface 생성은 vulkan_instance_<platform>.cpp가
// 맡고, 이 파일은 vulkan_instance.hpp에 선언된 두 함수만 부른다.

namespace nxt::graphics::vulkan {

namespace {

/// @brief 애플리케이션 이름이다. 앱의 몫이라 InstanceDesc가 생기는 A6에서 옮긴다.
constexpr const char* kApplicationName = "NXT Sandbox";

/**
 * @brief 루트 project()의 버전을 Vulkan packed 버전으로 만든다.
 *
 * 이름과 버전을 코드에 박지 않고 CMake가 project()에서 주입한다. 그래서 "0.1.0"
 * 문자열을 런타임에 파싱하지 않는다 (graphics/CMakeLists.txt).
 */
[[nodiscard]]
std::uint32_t projectVersion() noexcept {
    return vk::makeApiVersion(0, NXT_PROJECT_VERSION_MAJOR, NXT_PROJECT_VERSION_MINOR, NXT_PROJECT_VERSION_PATCH);
}

/**
 * @brief VK_EXT_debug_utils 메시지를 core 로그로 흘린다.
 *
 * 검증 레이어 메시지를 별도 출력 경로로 보내지 않는다 (rhi.md §7). 레이어가
 * 꺼져 있는 A4에서는 아무 메시지도 오지 않고, A6에서 레이어를 켜면 살아난다.
 */
VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    vk::DebugUtilsMessageTypeFlagsEXT type,
                                                    const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
                                                    void* userData) noexcept {
    (void)type;
    (void)userData;

    // 심각도 비트는 값이 클수록 심각하다. (Verbose < Info < Warning < Error)
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        NXT_LOG_ERROR(Graphics, Vulkan, "{}", callbackData->pMessage);
    } else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        NXT_LOG_WARN(Graphics, Vulkan, "{}", callbackData->pMessage);
    } else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        NXT_LOG_INFO(Graphics, Vulkan, "{}", callbackData->pMessage);
    } else {
        NXT_LOG_TRACE(Graphics, Vulkan, "{}", callbackData->pMessage);
    }

    return vk::False;
}

/**
 * @brief 요구한 instance extension이 모두 있는지 확인한다.
 *
 * 없는 것이 있으면 그 이름을 남기고 종료한다. 로더가 조용히 무시하고
 * createInstance가 실패하면 "무엇이 없어서"인지 알기 어렵기 때문이다 (rhi.md §7).
 */
void requireInstanceExtensions(const std::vector<const char*>& required) {
    const auto enumerated = vk::enumerateInstanceExtensionProperties();
    VK_CHECK(enumerated.result);
    const std::vector<vk::ExtensionProperties>& available = enumerated.value;

    for (const char* name : required) {
        const bool supported = std::any_of(available.begin(), available.end(), [name](const auto& property) {
            return std::strcmp(property.extensionName, name) == 0;
        });

        if (!supported) {
            const std::string message = std::format("required Vulkan instance extension is missing: {}", name);
            ::nxt::core::diag::reportAssertFailure(
                {"instance extension", message.c_str(), __FILE__, __func__, __LINE__});
        }

        NXT_LOG_DEBUG(Graphics, Vulkan, "instance extension available: {}", name);
    }
}

/**
 * @brief vk::Instance에 debug messenger를 붙인다.
 *
 * 실패하면 종료한다. 메신저는 instance보다 먼저 파괴해야 하므로 소유자가 멤버로
 * 들고 소멸자에서 정리한다.
 */
[[nodiscard]]
vk::DebugUtilsMessengerEXT createDebugMessenger(vk::Instance instance) {
    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = &debugUtilsCallback;

    const auto created = instance.createDebugUtilsMessengerEXT(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

} // namespace

std::unique_ptr<VulkanInstance> VulkanInstance::create() {
    // 동적 디스패처는 instance가 없어도 전역 함수를 부를 수 있어야 한다.
    // 링크한 vulkan-1.lib의 vkGetInstanceProcAddr로 시작하고, instance가 생기면
    // 그 주소로 다시 채운다. 두 번 중 하나라도 빠지면 첫 호출에서 죽는다.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(::vkGetInstanceProcAddr);

    // surface extension은 플랫폼마다 다르다. 이름만 platform 구현에서 받아온다.
    const std::vector<const char*> extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        platformSurfaceExtension(),
    };
    requireInstanceExtensions(extensions);

    vk::ApplicationInfo applicationInfo{};
    applicationInfo.pApplicationName = kApplicationName;
    applicationInfo.applicationVersion = projectVersion();
    applicationInfo.pEngineName = NXT_PROJECT_NAME;
    applicationInfo.engineVersion = projectVersion();
    applicationInfo.apiVersion = vk::ApiVersion14;

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    const auto created = vk::createInstance(createInfo);
    VK_CHECK(created.result);
    const vk::Instance instance = created.value;

    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다.
    auto result = std::unique_ptr<VulkanInstance>(new VulkanInstance(instance));
    result->debugMessenger_ = createDebugMessenger(instance);

    NXT_LOG_INFO(Graphics, Vulkan, "vulkan instance created, extensions={}", extensions.size());
    return result;
}

VulkanInstance::VulkanInstance(vk::Instance instance) noexcept : instance_{instance} {}

VulkanInstance::~VulkanInstance() {
    if (debugMessenger_) {
        instance_.destroyDebugUtilsMessengerEXT(debugMessenger_);
    }

    for (const vk::SurfaceKHR surface : surfaces_) {
        instance_.destroySurfaceKHR(surface);
    }
    surfaces_.clear();

    instance_.destroy();
}

rhi::Surface VulkanInstance::createSurface(const platform::window::NativeWindow& window) {
    const vk::SurfaceKHR surface = createPlatformSurface(instance_, window);

    surfaces_.push_back(surface);
    NXT_LOG_DEBUG(Graphics, Vulkan, "surface created");
    return fromVkHandle<rhi::SurfaceTag>(surface);
}

void VulkanInstance::destroySurface(const rhi::Surface& surface) {
    const vk::SurfaceKHR vulkanSurface = toVkHandle<vk::SurfaceKHR>(surface);
    const auto found = std::find(surfaces_.begin(), surfaces_.end(), vulkanSurface);
    NXT_ASSERT_MSG(found != surfaces_.end(), "이 인스턴스가 만들지 않은 surface를 파괴하려 했다.");
    if (found == surfaces_.end()) {
        return;
    }

    instance_.destroySurfaceKHR(vulkanSurface);
    surfaces_.erase(found);
    NXT_LOG_DEBUG(Graphics, Vulkan, "surface destroyed");
}

std::unique_ptr<rhi::Device> VulkanInstance::createDevice(const rhi::DeviceDesc& desc) {
    NXT_ASSERT_MSG(desc.surface.valid(), "device 생성에는 유효한 surface가 필요하다.");
    const vk::SurfaceKHR surface = toVkHandle<vk::SurfaceKHR>(desc.surface);
    return VulkanDevice::create(instance_, surface);
}

} // namespace nxt::graphics::vulkan

// Vulkan-Hpp의 디스패처 객체는 정의가 필요한 extern 변수다 (vulkan_hpp_macros.hpp).
// 이 매크로를 프로그램 전체에서 한 번만 펼쳐야 하므로, 디스패처를 초기화하는 이 TU가
// 그 자리를 맡는다. 다른 파일에 또 넣으면 중복 정의로 링크가 실패한다.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace nxt::graphics::rhi {

std::unique_ptr<Instance> Instance::create() {
    return vulkan::VulkanInstance::create();
}

} // namespace nxt::graphics::rhi
