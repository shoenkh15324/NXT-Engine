#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/resource/image.hpp>

namespace nxt::graphics::vulkan {

namespace {

/**
 * @brief 형식에서 image view의 aspect mask를 고른다.
 *
 * 깊이 전용 형식은 depth, 깊이·스텐실 결합 형식은 둘 다, 그 밖의 형식은 color다.
 * 형식 전체를 나열하지 않고 default로 색을 고르는 이유는 색 형식이 계속 늘어나도
 * 이 함수가 바뀌지 않기 때문이다. 깊이 형식은 Vulkan이 정한 목록이 고정되어 있다.
 *
 * @param[in] format view를 만들 이미지의 형식.
 * @return 그 형식에 맞는 aspect mask.
 */
[[nodiscard]]
vk::ImageAspectFlags imageAspectMask(rhi::Format format) noexcept {
    switch (format) {
        case rhi::Format::D32SFloat:
            return vk::ImageAspectFlagBits::eDepth;
        case rhi::Format::D24UnormS8Uint:
        case rhi::Format::D32SFloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            return vk::ImageAspectFlagBits::eColor;
    }
}

} // namespace

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

ImageAllocation createImage(const Device& device, const ImageDesc& desc) {
    NXT_ASSERT_MSG((desc.extent.width > 0) && (desc.extent.height > 0), "크기가 0인 이미지는 만들 수 없다.");
    NXT_ASSERT_MSG(desc.format != Format::Undefined, "형식이 정해지지 않은 이미지는 만들 수 없다.");
    NXT_ASSERT_MSG(static_cast<std::uint32_t>(desc.usage) != 0, "용도가 없는 이미지는 만들 수 없다.");

    const vk::Device vkDevice = vulkan::toVulkanDevice(device).device();

    vk::ImageCreateInfo createInfo{};
    createInfo.imageType = vk::ImageType::e2D;
    createInfo.format = vulkan::toVulkan(desc.format);
    createInfo.extent = vk::Extent3D{desc.extent.width, desc.extent.height, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = vk::SampleCountFlagBits::e1;
    createInfo.tiling = vk::ImageTiling::eOptimal;
    createInfo.usage = vulkan::toVulkan(desc.usage);
    // Phase 0은 큐가 하나뿐이라 소유권 이전이 없다. exclusive면 충분하다 (rhi.md §8).
    createInfo.sharingMode = vk::SharingMode::eExclusive;
    // 첫 사용 전 layout은 render pass의 initialLayout이 정한다. Phase 0은 eUndefined에서
    // 시작하고, 복사 경로가 생기면 그때 barrier로 바꾼다 (frame.md §8).
    createInfo.initialLayout = vk::ImageLayout::eUndefined;

    const auto created = vkDevice.createImage(createInfo);
    VK_CHECK(created.result);

    // 정렬 때문에 요구 크기가 픽셀 크기에서 계산한 값보다 클 수 있다. 할당은
    // requirements.size로 하고, memory type도 이 이미지가 쓸 수 있는 비트 안에서만 고른다.
    const vk::MemoryRequirements requirements = vkDevice.getImageMemoryRequirements(created.value);
    const Memory memory = allocateMemory(device, requirements.size, desc.memoryProperty, requirements.memoryTypeBits);

    const vk::DeviceMemory vkMemory = vulkan::toVkHandle<vk::DeviceMemory>(memory);
    VK_CHECK(vkDevice.bindImageMemory(created.value, vkMemory, 0));

    const ImageAllocation allocation{vulkan::fromVkHandle<ImageTag>(created.value), memory};
    NXT_LOG_DEBUG(Graphics, Vulkan, "image created: image={:#x} memory={:#x} extent={}x{} format={} allocated={}",
                  allocation.image.value(), memory.value(), desc.extent.width, desc.extent.height,
                  vk::to_string(createInfo.format), requirements.size);
    return allocation;
}

void destroyImage(const Device& device, const ImageAllocation& allocation) {
    NXT_ASSERT_MSG(allocation.image.valid(), "유효하지 않은 image를 해제하려 했다.");
    NXT_ASSERT_MSG(allocation.memory.valid(), "유효하지 않은 memory를 해제하려 했다.");

    const vk::Device vkDevice = vulkan::toVulkanDevice(device).device();

    // 메모리를 먼저 해제하면 이미지가 죽은 메모리를 가리킨다. 이미지가 먼저다.
    // 이 이미지에 만든 view가 남아 있으면 Vulkan이 파괴를 거부하므로 호출자가 먼저 지운다.
    const vk::Image vkImage = vulkan::toVkHandle<vk::Image>(allocation.image);
    vkDevice.destroyImage(vkImage);
    NXT_LOG_DEBUG(Graphics, Vulkan, "image destroyed: image={:#x}", allocation.image.value());

    freeMemory(device, allocation.memory);
}

ImageView createImageView(const Device& device, const ImageViewDesc& desc) {
    NXT_ASSERT_MSG(desc.image.valid(), "유효하지 않은 image로 image view를 만들려 했다.");
    NXT_ASSERT_MSG(desc.format != Format::Undefined, "형식이 정해지지 않은 image view는 만들 수 없다.");

    vk::ImageViewCreateInfo createInfo{};
    createInfo.image = vulkan::toVkHandle<vk::Image>(desc.image);
    createInfo.viewType = vk::ImageViewType::e2D;
    createInfo.format = vulkan::toVulkan(desc.format);
    createInfo.subresourceRange.aspectMask = vulkan::imageAspectMask(desc.format);
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    const auto created = vulkan::toVulkanDevice(device).device().createImageView(createInfo);
    VK_CHECK(created.result);

    const ImageView view = vulkan::fromVkHandle<ImageViewTag>(created.value);
    NXT_LOG_DEBUG(Graphics, Vulkan, "image view created: view={:#x} image={:#x} format={}", view.value(),
                  desc.image.value(), vk::to_string(createInfo.format));
    return view;
}

void destroyImageView(const Device& device, const ImageView& view) {
    NXT_ASSERT_MSG(view.valid(), "유효하지 않은 image view를 해제하려 했다.");

    const vk::ImageView vkView = vulkan::toVkHandle<vk::ImageView>(view);
    vulkan::toVulkanDevice(device).device().destroyImageView(vkView);
    NXT_LOG_DEBUG(Graphics, Vulkan, "image view destroyed: view={:#x}", view.value());
}

} // namespace nxt::graphics::rhi
