#include <algorithm>
#include <cstdint>
#include <limits>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/backends/vulkan/frame/vulkan_swapchain.hpp>
#include <optional>
#include <vector>

namespace nxt::graphics::vulkan {

namespace {

/// @brief 무한정 기다린다 - CPU가 GPU보다 앞서가지 않는 쪽이 옳다고 가정한다 (frame.md §5).
constexpr std::uint64_t kWaitForever = std::numeric_limits<std::uint64_t>::max();

/// @brief 고른 surface 형식과 그에 대응하는 rhi::Format이다.
struct ChosenFormat {
    vk::SurfaceFormatKHR surfaceFormat;
    rhi::Format format;
};

/**
 * @brief swapchain이 쓸 surface 형식을 고른다.
 *
 * rhi::Format으로 표현할 수 있는 형식만 후보로 둔다. surface가 무엇을 주든 되돌릴 수
 * 없는 형식을 고르면 format()이 거짓말이 되기 때문이다. sRGB를 먼저 고르는 이유는
 * 선형 값이 화면에서 올바르게 보이려면 sRGB 스왑체인이 필요해서다.
 *
 * colorSpace는 고른 항목이 제시한 값을 그대로 쓴다 — 형식과 색 공간은 한 쌍이다.
 *
 * @param[in] formats surface가 지원하는 형식 목록.
 * @return 고른 형식. 후보가 없으면 종료한다.
 */
[[nodiscard]]
ChosenFormat chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    constexpr rhi::Format kPreferredFormats[] = {rhi::Format::B8G8R8A8Srgb, rhi::Format::B8G8R8A8Unorm,
                                                 rhi::Format::R8G8B8A8Unorm};

    for (const rhi::Format preferred : kPreferredFormats) {
        for (const vk::SurfaceFormatKHR& available : formats) {
            if (available.format == toVulkan(preferred)) {
                return {available, preferred};
            }
        }
    }

    ::nxt::core::diag::reportAssertFailure({"surface format",
                                            "surface가 지원하는 형식 중 rhi::Format으로 표현할 수 있는 것이 없다.",
                                            __FILE__, __func__, __LINE__});
}

/**
 * @brief 요청한 present 방식을 쓸 수 있으면 그대로, 아니면 Fifo로 되돌린다.
 *
 * Fifo는 spec이 모든 surface에서 지원하도록 요구하므로 항상 안전한 기본값이다.
 * 조용히 바꾸지 않고 남기는 이유는 "왜 VSync가 걸렸는가"를 추적할 수 있어야 해서다.
 *
 * @param[in] modes     surface가 지원하는 방식 목록.
 * @param[in] requested 요청한 방식.
 * @return 실제로 쓸 방식.
 */
[[nodiscard]]
vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& modes, rhi::PresentMode requested) {
    const vk::PresentModeKHR wanted = toVulkan(requested);
    if (std::find(modes.begin(), modes.end(), wanted) != modes.end()) {
        return wanted;
    }

    NXT_LOG_WARN(Graphics, Vulkan, "present mode not supported, falling back to fifo, requested={}",
                 vk::to_string(wanted));
    return vk::PresentModeKHR::eFifo;
}

/**
 * @brief swapchain 이미지 개수를 고른다.
 *
 * 트리플 버퍼링이 기본이다. 더블은 VSync를 한 번 놓치면 프레임률이 절반으로 떨어진다
 * (frame.md §5). maxImageCount가 0이면 상한이 없다는 뜻이다.
 *
 * @param[in] capabilities surface의 제약.
 * @return minImageCount로 쓸 값.
 */
[[nodiscard]]
std::uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
    constexpr std::uint32_t kPreferredImageCount = 3;

    std::uint32_t imageCount = std::max(kPreferredImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }
    return imageCount;
}

/**
 * @brief 지원하는 composite alpha 중 하나를 고른다.
 *
 * Opaque가 기본이다 — Phase 0의 창은 알파를 섞지 않고 전체를 덮는다.
 *
 * @param[in] supported surface가 지원하는 비트 집합.
 * @return 고른 값. 지원하는 것이 없으면 종료한다.
 */
[[nodiscard]]
vk::CompositeAlphaFlagBitsKHR chooseCompositeAlpha(vk::CompositeAlphaFlagsKHR supported) {
    constexpr vk::CompositeAlphaFlagBitsKHR kCandidates[] = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
    };

    for (const vk::CompositeAlphaFlagBitsKHR candidate : kCandidates) {
        if (supported & candidate) {
            return candidate;
        }
    }

    ::nxt::core::diag::reportAssertFailure(
        {"composite alpha", "surface가 지원하는 composite alpha가 없다.", __FILE__, __func__, __LINE__});
}

/**
 * @brief surface가 알려 주는 이미지 크기를 꺼낸다.
 *
 * Win32는 currentExtent에 창의 실제 크기를 넣어 준다. 최소화 상태에서는 0x0이며,
 * 이때는 swapchain을 만들 수 없으므로 0을 그대로 돌려주고 호출자가 판단한다.
 *
 * @param[in] capabilities surface의 제약.
 * @return 이미지 크기. 창에 그릴 영역이 없으면 0x0.
 */
[[nodiscard]]
rhi::Extent2D surfaceExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    constexpr std::uint32_t kUndefinedExtent = std::numeric_limits<std::uint32_t>::max();

    if (capabilities.currentExtent.width == kUndefinedExtent) {
        // surface가 크기를 정하지 않는다 (X11 등). SwapchainDesc에 크기가 없으므로 채울
        // 값이 없다 - Linux 백엔드가 붙을 때 desc에 창 크기를 넣는다.
        ::nxt::core::diag::reportAssertFailure({"extent",
                                                "surface가 extent를 정하지 않는다. SwapchainDesc에 크기가 필요하다.",
                                                __FILE__, __func__, __LINE__});
    }

    return {capabilities.currentExtent.width, capabilities.currentExtent.height};
}

} // namespace

std::unique_ptr<VulkanSwapchain> VulkanSwapchain::create(const VulkanDevice& device, const rhi::SwapchainDesc& desc) {
    NXT_ASSERT_MSG(desc.surface.valid(), "유효하지 않은 surface로 swapchain을 만들려했다.");

    const vk::SurfaceKHR surface = toVkHandle<vk::SurfaceKHR>(desc.surface);

    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다 (VulkanDevice와 같은 이유).
    return std::unique_ptr<VulkanSwapchain>(new VulkanSwapchain(device, surface, desc.presentMode));
}

VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device, vk::SurfaceKHR surface, rhi::PresentMode presentMode)
    : device_{device}, surface_{surface}, presentMode_{presentMode} {
    if (!createSwapchain(vk::SwapchainKHR{})) {
        ::nxt::core::diag::reportAssertFailure(
            {"swapchain", "창에 그릴 영역이 없어 swapchain을 만들 수 없다.", __FILE__, __func__, __LINE__});
    }
}

VulkanSwapchain::~VulkanSwapchain() {
    destroyImageViews();
    if (swapchain_) {
        device_.device().destroySwapchainKHR(swapchain_);
    }
    NXT_LOG_DEBUG(Graphics, Vulkan, "swapchain destroyed");
}

bool VulkanSwapchain::createSwapchain(vk::SwapchainKHR oldSwapchain) {
    const vk::PhysicalDevice physicalDevice = device_.physicalDevice();

    const auto capabilitiesResult = physicalDevice.getSurfaceCapabilitiesKHR(surface_);
    VK_CHECK(capabilitiesResult.result);
    const vk::SurfaceCapabilitiesKHR capabilities = capabilitiesResult.value;

    // 크기를 먼저 본다. 여기서 돌아가면 아무것도 바꾸지 않으므로 old swapchain이 살아남는다.
    const rhi::Extent2D extent = surfaceExtent(capabilities);
    if ((extent.width == 0) || (extent.height == 0)) {
        return false;
    }

    const auto formatsResult = physicalDevice.getSurfaceFormatsKHR(surface_);
    VK_CHECK(formatsResult.result);

    const auto modesResult = physicalDevice.getSurfacePresentModesKHR(surface_);
    VK_CHECK(modesResult.result);

    const ChosenFormat chosenFormat = chooseSurfaceFormat(formatsResult.value);
    const vk::PresentModeKHR presentMode = choosePresentMode(modesResult.value, presentMode_);

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface = surface_;
    createInfo.minImageCount = chooseImageCount(capabilities);
    createInfo.imageFormat = chosenFormat.surfaceFormat.format;
    createInfo.imageColorSpace = chosenFormat.surfaceFormat.colorSpace;
    createInfo.imageExtent = vk::Extent2D{extent.width, extent.height};
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    // queue family를 하나만 쓰므로 exclusive로 충분하다 (rhi.md §8).
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = chooseCompositeAlpha(capabilities.supportedCompositeAlpha);
    createInfo.presentMode = presentMode;
    createInfo.clipped = vk::True;
    createInfo.oldSwapchain = oldSwapchain;

    const auto created = device_.device().createSwapchainKHR(createInfo);
    VK_CHECK(created.result);

    // 새 swapchain이 만들어졌으므로 이전 image view는 더 이상 필요 없다.
    destroyImageViews();

    swapchain_ = created.value;
    format_ = chosenFormat.format;
    extent_ = extent;

    const auto images = device_.device().getSwapchainImagesKHR(swapchain_);
    VK_CHECK(images.result);

    imageViews_.reserve(images.value.size());
    for (const vk::Image image : images.value) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = createInfo.imageFormat;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        const auto view = device_.device().createImageView(viewInfo);
        VK_CHECK(view.result);
        imageViews_.push_back(view.value);
    }

    NXT_LOG_DEBUG(Graphics, Vulkan, "swapchain created: images={} extent={}x{} format={} present={}",
                  imageViews_.size(), extent_.width, extent_.height, vk::to_string(createInfo.imageFormat),
                  vk::to_string(presentMode));
    return true;
}

void VulkanSwapchain::destroyImageViews() noexcept {
    for (const vk::ImageView view : imageViews_) {
        device_.device().destroyImageView(view);
    }
    imageViews_.clear();
}

std::optional<std::uint32_t> VulkanSwapchain::acquireNextImage(const rhi::Semaphore& signalSemaphore) {
    NXT_ASSERT_MSG(signalSemaphore.valid(), "유효하지 않은 semaphore로 이미지를 얻으려 했다.");

    const vk::Semaphore semaphore = toVkHandle<vk::Semaphore>(signalSemaphore);
    const auto acquired = device_.device().acquireNextImageKHR(swapchain_, kWaitForever, semaphore, vk::Fence{});

    if (acquired.result == vk::Result::eErrorOutOfDateKHR) {
        NXT_LOG_DEBUG(Graphics, Vulkan, "swapchain out of date: acquire");
        return std::nullopt;
    }

    // eSuboptimalKHR은 이미지가 유효하다는 뜻이다. 그리게 두고 다음 크기 변경에서 맞춘다.
    if (acquired.result == vk::Result::eSuboptimalKHR) {
        NXT_LOG_WARN(Graphics, Vulkan, "swapchain suboptimal: image={}", acquired.value);
        return acquired.value;
    }

    VK_CHECK(acquired.result);
    return acquired.value;
}

bool VulkanSwapchain::present(std::uint32_t imageIndex, const rhi::Semaphore& waitSemaphore) {
    NXT_ASSERT_MSG(imageIndex < imageViews_.size(), "swapchain 이미지 인덱스가 범위를 벗어났다.");
    NXT_ASSERT_MSG(waitSemaphore.valid(), "유효하지 않은 semaphore로 제시하려 했다.");

    const vk::Semaphore wait = toVkHandle<vk::Semaphore>(waitSemaphore);

    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &wait;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    const vk::Result result = toVkHandle<vk::Queue>(device_.presentQueue()).presentKHR(presentInfo);
    if ((result == vk::Result::eErrorOutOfDateKHR) || (result == vk::Result::eSuboptimalKHR)) {
        NXT_LOG_DEBUG(Graphics, Vulkan, "swapchain out of date: present");
        return false;
    }

    VK_CHECK(result);
    return true;
}

rhi::ImageView VulkanSwapchain::imageView(std::uint32_t imageIndex) const {
    NXT_ASSERT_MSG(imageIndex < imageViews_.size(), "swapchain 이미지 인덱스가 범위를 벗어났다.");
    return fromVkHandle<rhi::ImageViewTag>(imageViews_[imageIndex]);
}

rhi::Format VulkanSwapchain::format() const noexcept {
    return format_;
}

std::uint32_t VulkanSwapchain::imageCount() const noexcept {
    return static_cast<std::uint32_t>(imageViews_.size());
}

rhi::Extent2D VulkanSwapchain::extent() const noexcept {
    return extent_;
}

void VulkanSwapchain::recreate() {
    // 호출자는 in-flight fence를 먼저 기다린다 (frame.md §6). swapchain은 프레임 수명을
    // 모르므로 여기서 기다리지 않는다.
    const vk::SwapchainKHR oldSwapchain = swapchain_;

    if (!createSwapchain(oldSwapchain)) {
        // 창이 최소화되어 그릴 영역이 없다. 이전 swapchain을 그대로 두고 다음 기회를 기다린다.
        NXT_LOG_DEBUG(Graphics, Vulkan, "swapchain recreate skipped: window has no area");
        return;
    }

    // 새 swapchain이 만들어지면 oldSwapchain은 retire된다. 여기서 파괴해도 안전하다.
    if (oldSwapchain) {
        device_.device().destroySwapchainKHR(oldSwapchain);
    }
}

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

std::unique_ptr<Swapchain> Swapchain::create(const Device& device, const SwapchainDesc& desc) {
    return vulkan::VulkanSwapchain::create(vulkan::toVulkanDevice(device), desc);
}

} // namespace nxt::graphics::rhi
