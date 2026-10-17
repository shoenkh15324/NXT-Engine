#pragma once

#include <memory>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/rhi/core/device.hpp>
#include <nxt/graphics/rhi/core/instance.hpp>
#include <vector>

namespace nxt::graphics::vulkan {

/**
 * @brief rhi::Instance의 Vulkan 구현이다.
 *
 * vk::Instance를 만들고, 창 위에 그릴 surface와 GPU 작업을 수행할 device를
 * 관리한다. vk:: 타입이 이 헤더에 나타나는 것은 backends/vulkan/ 안이기 때문에
 * 허용된다 (rhi.md §4). rhi·renderer·engine은 이 헤더를 include하지 않는다.
 *
 * @note rhi::Instance::create()가 이 클래스를 만들어 unique_ptr<rhi::Instance>로
 *       돌려준다. 직접 만들지 않는다 — 생성자가 private이다.
 * @note Thread safety: thread-safe하지 않다. 생성과 파괴는 같은 스레드에서 한다.
 */
class VulkanInstance final : public rhi::Instance {
public:
    /**
     * @brief vk::Instance와 debug messenger를 만든다.
     *
     * 필요한 instance extension을 확인하고 켠다. VK_KHR_surface와
     * VK_EXT_debug_utils는 공통이고, 나머지 surface extension은 플랫폼 구현이
     * 정한다 (vulkan_instance_<platform>.cpp). 실패하면 원인을 로그로 남기고
     * 종료한다 — GPU 오류는 복구 불가능하다 (rhi.md §7).
     *
     * @return 만들어진 인스턴스.
     */
    [[nodiscard]]
    static std::unique_ptr<VulkanInstance> create();

    /**
     * @brief 남은 surface와 debug messenger를 정리한 뒤 vk::Instance를 파괴한다.
     *
     * @note vk::Instance는 vk::SurfaceKHR와 vk::DebugUtilsMessengerEXT보다 오래
     *       살아야 한다. 그래서 파괴 순서를 소멸자가 강제한다.
     */
    ~VulkanInstance() override;

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    /**
     * @brief 창 위에 그릴 surface를 만든다.
     *
     * 실제 생성은 플랫폼 구현이 맡는다 (vulkan_instance_<platform>.cpp). 이 클래스는
     * 만들어진 surface를 소유 목록에 넣어 소멸자에서 함께 정리한다.
     *
     * @param[in] window 창의 원시 핸들. 플랫폼별 필드는 NativeWindow가 흡수한다.
     * @return 만들어진 surface.
     */
    [[nodiscard]]
    rhi::Surface createSurface(const platform::window::NativeWindow& window) override;

    /**
     * @brief surface를 파괴하고 목록에서 뺀다.
     *
     * @param[in] surface 파괴할 surface.
     */
    void destroySurface(const rhi::Surface& surface) override;

    /**
     * @brief 물리 디바이스를 골라 논리 디바이스를 만든다.
     *
     * desc.surface에 present할 수 있는 queue family를 가진 물리 디바이스를 고른다
     * (rhi.md §8). Device를 만들 수 있는 유일한 지점이다 (phase0.md A4).
     *
     * @param[in] desc surface가 담긴 device 설정.
     * @return 만들어진 device.
     */
    [[nodiscard]]
    std::unique_ptr<rhi::Device> createDevice(const rhi::DeviceDesc& desc) override;

private:
    /**
     * @brief 이미 만든 vk::Instance를 감싼다.
     *
     * @param[in] instance create()가 만든 vk::Instance.
     */
    explicit VulkanInstance(vk::Instance instance) noexcept;

    /// @brief 이 인스턴스의 vk::Instance. 파괴는 소멸자가 맡는다.
    vk::Instance instance_{};

    /// @brief VK_EXT_debug_utils 메시지 수신자. instance보다 먼저 파괴한다.
    vk::DebugUtilsMessengerEXT debugMessenger_{};

    /// @brief 이 인스턴스가 만든 surface들. 소멸자에서 함께 파괴한다.
    std::vector<vk::SurfaceKHR> surfaces_;
};

/**
 * @brief 이 플랫폼이 요구하는 surface extension 이름을 돌려준다.
 *
 * 구현은 vulkan_instance_<platform>.cpp 중 CMake가 고른 하나에 있다
 * (backends/vulkan/CMakeLists.txt). instance를 만들기 전에 확장 목록을 조립해야
 * 하므로 createPlatformSurface와 분리돼 있다.
 *
 * @return 확장 이름 (예: VK_KHR_win32_surface). 항상 유효하다.
 */
[[nodiscard]]
const char* platformSurfaceExtension() noexcept;

/**
 * @brief 창의 원시 핸들로 이 플랫폼의 surface를 만든다.
 *
 * 구현은 vulkan_instance_<platform>.cpp에 있다. windows.h · xcb.h 같은 플랫폼
 * 헤더는 그 파일 안에만 들어오므로 vulkan_instance.cpp는 플랫폼을 알지 않는다.
 *
 * @param[in] instance surface를 만들 vk::Instance.
 * @param[in] window 창의 원시 핸들. 플랫폼별 차이는 NativeWindow가 흡수한다
 *                   (window.hpp — Win32는 handle=HWND, X11은 display=xcb_connection_t*).
 * @return 만들어진 surface.
 */
[[nodiscard]]
vk::SurfaceKHR createPlatformSurface(vk::Instance instance, const platform::window::NativeWindow& window);

} // namespace nxt::graphics::vulkan
