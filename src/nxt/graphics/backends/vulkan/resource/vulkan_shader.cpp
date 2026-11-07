#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/resource/shader.hpp>
#include <span>

namespace nxt::graphics::rhi {

Shader createShader(const Device& device, std::span<const std::uint32_t> code) {
    NXT_ASSERT_MSG(!code.empty(), "빈 SPIR-V 코드로 shader를 만들 수 없다.");

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size_bytes();
    createInfo.pCode = code.data();

    const auto created = vulkan::toVulkanDevice(device).device().createShaderModule(createInfo);
    VK_CHECK(created.result);

    const Shader shader = vulkan::fromVkHandle<ShaderTag>(created.value);
    NXT_LOG_DEBUG(Graphics, Vulkan, "shader created: shader={:#x} bytes={}", shader.value(), code.size_bytes());
    return shader;
}

void destroyShader(const Device& device, const Shader& shader) {
    NXT_ASSERT_MSG(shader.valid(), "유효하지 않은 shader를 해제하려 했다.");

    const vk::ShaderModule vkShader = vulkan::toVkHandle<vk::ShaderModule>(shader);
    vulkan::toVulkanDevice(device).device().destroyShaderModule(vkShader);
    NXT_LOG_DEBUG(Graphics, Vulkan, "shader destroyed: shader={:#x}", shader.value());
}

} // namespace nxt::graphics::rhi
