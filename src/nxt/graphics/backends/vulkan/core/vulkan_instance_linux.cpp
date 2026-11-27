#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_instance.hpp>
#include <wayland-client.h>

// VulkanInstance의 Linux 절반이다. CMake가 이 플랫폼에서만 이 파일을 빌드한다
// (backends/vulkan/CMakeLists.txt). vulkan_instance.cpp는 여기 있는 두 함수의
// 선언만 알고, wayland-client.h를 알지 않는다.
//
// VK_USE_PLATFORM_WAYLAND_KHR은 nxt_graphics 타깃 정의로 걸려 있다.
// vulkan.hpp가 이 매크로 때문에 wayland-client.h를 include하므로 타깃 전체에 필요하다.

namespace nxt::graphics::vulkan {

const char* platformSurfaceExtension() noexcept {
    return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
}

vk::SurfaceKHR createPlatformSurface(vk::Instance instance, const platform::window::NativeWindow& window) {
    // Wayland의 Vulkan surface는 display와 surface를 함께 요구한다.
    // 두 포인터는 NativeWindow가 그대로 전달한다 (window.hpp).
    vk::WaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.display = static_cast<wl_display*>(window.display);
    createInfo.surface = static_cast<wl_surface*>(window.handle);

    const auto created = instance.createWaylandSurfaceKHR(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

} // namespace nxt::graphics::vulkan
