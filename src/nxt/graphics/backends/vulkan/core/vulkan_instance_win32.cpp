#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_instance.hpp>
#include <windows.h>

// VulkanInstance의 Win32 절반이다. CMake가 이 플랫폼에서만 이 파일을 빌드한다
// (backends/vulkan/CMakeLists.txt). vulkan_instance.cpp는 여기 있는 두 함수의
// 선언만 알고, windows.h를 알지 않는다.
//
// NOMINMAX · WIN32_LEAN_AND_MEAN은 nxt_graphics 타깃 정의로 걸려 있다.
// vulkan.hpp가 VK_USE_PLATFORM_WIN32_KHR 때문에 windows.h를 include하므로
// 특정 파일이 아니라 타깃 전체에 필요하다.

namespace nxt::graphics::vulkan {

const char* platformSurfaceExtension() noexcept {
    return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
}

vk::SurfaceKHR createPlatformSurface(vk::Instance instance, const platform::window::NativeWindow& window) {
    vk::Win32SurfaceCreateInfoKHR createInfo{};
    // GetModuleHandle(nullptr)로 hinstance를 대체할 수 있다 (window.hpp).
    createInfo.hinstance = ::GetModuleHandleW(nullptr);
    createInfo.hwnd = static_cast<HWND>(window.handle);

    const auto created = instance.createWin32SurfaceKHR(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

} // namespace nxt::graphics::vulkan
