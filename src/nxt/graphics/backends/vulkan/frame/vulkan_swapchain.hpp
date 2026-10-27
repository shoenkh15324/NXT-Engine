#pragma once

#include <cstdint>
#include <memory>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/rhi/frame/swapchain.hpp>
#include <optional>
#include <vector>

namespace nxt::graphics::vulkan {

class VulkanDevice;

/**
 * @brief rhi::Swapchain의 Vulkan 구현이다.
 *
 * VkSwapchainKHR과 그 이미지들의 view를 소유한다. 제시할 대상(Surface)과 제시에 쓸
 * 큐는 디바이스가 소유하므로 여기서는 빌려 쓴다.
 *
 * @note rhi::Swapchain::create()가 이 클래스를 만들어 unique_ptr<rhi::Swapchain>으로
 *       돌려준다. 직접 만들지 않는다 — 생성자가 private이다.
 * @note 소유권: swapchain과 image view는 이 객체의 것이다. Surface와 Device보다 먼저
 *       파괴해야 한다.
 * @note Thread safety: thread-safe하지 않다. acquire · present · recreate는 같은
 *       스레드에서 순서대로 부른다.
 */
class VulkanSwapchain final : public rhi::Swapchain {
public:
    /**
     * @brief surface에 맞는 swapchain과 image view를 만든다.
     *
     * @param[in] device swapchain을 만들 디바이스.
     * @param[in] desc   제시할 surface와 present 방식.
     * @return 만들어진 swapchain.
     */
    [[nodiscard]]
    static std::unique_ptr<VulkanSwapchain> create(const VulkanDevice& device, const rhi::SwapchainDesc& desc);

    /// @brief image view를 먼저 파괴한 뒤 swapchain을 파괴한다.
    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    [[nodiscard]]
    std::optional<std::uint32_t> acquireNextImage(const rhi::Semaphore& signalSemaphore) override;

    [[nodiscard]]
    bool present(std::uint32_t imageIndex, const rhi::Semaphore& waitSemaphore) override;

    [[nodiscard]]
    rhi::ImageView imageView(std::uint32_t imageIndex) const override;

    [[nodiscard]]
    rhi::Format format() const noexcept override;

    [[nodiscard]]
    std::uint32_t imageCount() const noexcept override;

    [[nodiscard]]
    rhi::Extent2D extent() const noexcept override;

    void recreate() override;

private:
    /**
     * @brief 이미 만든 surface에 묶인다. swapchain 생성은 createSwapchain()이 맡는다.
     *
     * @param[in] device      swapchain을 만들 디바이스.
     * @param[in] surface     제시할 대상.
     * @param[in] presentMode 요청할 present 방식.
     */
    VulkanSwapchain(const VulkanDevice& device, vk::SurfaceKHR surface, rhi::PresentMode presentMode);

    /**
     * @brief surface에 맞춰 swapchain과 image view를 (다시) 만든다.
     *
     * @param[in] oldSwapchain recreate()가 넘기는 이전 swapchain. 처음 만들 때는
     *                          유효하지 않은 핸들이다.
     * @return 창에 그릴 영역이 있으면 true. 없으면 false이며 아무것도 바꾸지 않았다.
     */
    [[nodiscard]]
    bool createSwapchain(vk::SwapchainKHR oldSwapchain);

    /// @brief 지금 image view들을 모두 파괴한다.
    void destroyImageViews() noexcept;

    /// @brief swapchain과 view를 만들 디바이스. 이 객체보다 오래 산다.
    const VulkanDevice& device_;

    /// @brief 제시할 대상 창이다. Instance가 소유한다.
    vk::SurfaceKHR surface_{};

    /// @brief 요청한 present 방식. recreate()할 때 지원 여부를 다시 확인한다.
    rhi::PresentMode presentMode_{rhi::PresentMode::Fifo};

    /// @brief Vulkan swapchain. 소멸자와 recreate()가 파괴한다.
    vk::SwapchainKHR swapchain_{};

    /// @brief 이미지마다 하나씩 있는 view. swapchain보다 먼저 파괴한다.
    std::vector<vk::ImageView> imageViews_{};

    /// @brief color attachment로 쓸 픽셀 형식이다.
    rhi::Format format_{rhi::Format::Undefined};

    /// @brief 현재 이미지의 크기이다.
    rhi::Extent2D extent_{};
};

} // namespace nxt::graphics::vulkan
