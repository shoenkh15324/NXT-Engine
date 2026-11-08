#include <array>
#include <cstddef>
#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/resource/pipeline.hpp>
#include <vector>

namespace nxt::graphics::vulkan {

namespace {

/**
 * @brief 파이프라인이 호환되어야 하는 색 render pass를 만든다.
 *
 * 파이프라인은 만들 때 render pass가 필요하고, 그 render pass는 그릴 때 encoder가
 * 쓰는 것과 호환되어야 한다. 그래서 frame/vulkan_command_buffer.cpp의
 * createColorRenderPass()와 부착물·subpass 구성이 같아야 한다. B4에서 깊이 부착물을
 * 넣을 때 두 함수를 함께 고쳐야 한다 (phase0.md B4).
 *
 * 이 render pass는 생성 호출이 끝나면 바로 파괴한다. 파이프라인은 render pass
 * 핸들을 보관하지 않고, 호환성은 구조로만 판정하기 때문이다.
 *
 * @param[in] device render pass를 만들 디바이스.
 * @param[in] format 색 부착물의 형식.
 * @return 만들어진 render pass.
 */
[[nodiscard]]
vk::RenderPass createCompatibleColorRenderPass(vk::Device device, rhi::Format format) {
    vk::AttachmentDescription attachment{};
    attachment.format = toVulkan(format);
    attachment.samples = vk::SampleCountFlagBits::e1;
    attachment.loadOp = vk::AttachmentLoadOp::eClear;
    attachment.storeOp = vk::AttachmentStoreOp::eStore;
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachment.initialLayout = vk::ImageLayout::eUndefined;
    attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    vk::RenderPassCreateInfo createInfo{};
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &attachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    const auto created = device.createRenderPass(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

/**
 * @brief descriptor set도 push constant도 없는 빈 pipeline layout을 만든다.
 *
 * Phase 0의 셰이더는 균일 상수를 받지 않는다. 그래서 setLayoutCount와
 * pushConstantRangeCount가 0인 layout이면 충분하다.
 *
 * @param[in] device layout을 만들 디바이스.
 * @return 만들어진 layout.
 */
[[nodiscard]]
vk::PipelineLayout createEmptyPipelineLayout(vk::Device device) {
    vk::PipelineLayoutCreateInfo createInfo{};
    const auto created = device.createPipelineLayout(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

/**
 * @brief vk::GraphicsPipelineCreateInfo가 가리킬 상태를 한 곳에 모은다.
 *
 * createInfo는 각 상태를 포인터로 가리키고, 정점 속성 배열은 vector가 소유한다.
 * 상태를 지역 변수로 흩어 두면 어느 배열이 먼저 사라지는지 추적해야 하고,
 * 구조체를 복사하면 createInfo의 포인터가 매달린다. 그래서 상태를 한 구조체가
 * 소유하고, createInfo()가 그 멤버만 가리키게 한다.
 *
 * @note 채운 뒤에는 이 구조체를 옮기지 않는다. 포인터가 자기 멤버를 가리킨다.
 */
struct PipelineStates {
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    vk::VertexInputBindingDescription binding{};
    std::vector<vk::VertexInputAttributeDescription> attributes;
    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    vk::PipelineViewportStateCreateInfo viewportState{};
    vk::PipelineRasterizationStateCreateInfo rasterization{};
    vk::PipelineMultisampleStateCreateInfo multisample{};
    vk::PipelineColorBlendAttachmentState blendAttachment{};
    vk::PipelineColorBlendStateCreateInfo colorBlend{};
    std::array<vk::DynamicState, 2> dynamicStates{};
    vk::PipelineDynamicStateCreateInfo dynamicState{};

    /**
     * @brief 모든 상태를 채운 뒤에 호출한다.
     *
     * @param[in] layout     파이프라인이 쓸 pipeline layout.
     * @param[in] renderPass 파이프라인이 호환되어야 하는 render pass.
     * @return 이 구조체의 상태를 가리키는 기술자.
     */
    [[nodiscard]]
    vk::GraphicsPipelineCreateInfo createInfo(vk::PipelineLayout layout, vk::RenderPass renderPass) const noexcept {
        vk::GraphicsPipelineCreateInfo info{};
        info.stageCount = static_cast<std::uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &rasterization;
        info.pMultisampleState = &multisample;
        // render pass에 깊이 부착물이 없으므로 null이어야 한다. B4에서 깊이를 넣으면
        // 이 자리와 양쪽 render pass가 함께 바뀐다.
        info.pDepthStencilState = nullptr;
        info.pColorBlendState = &colorBlend;
        info.pDynamicState = &dynamicState;
        info.layout = layout;
        info.renderPass = renderPass;
        info.subpass = 0;
        info.basePipelineHandle = nullptr;
        info.basePipelineIndex = -1;
        return info;
    }
};

/**
 * @brief 버텍스·프래그먼트 두 단계를 채운다.
 *
 * 진입점 이름은 SPIR-V의 OpEntryPoint와 같아야 한다. Phase 0의 셰이더는 glslc
 * 기본값인 "main"을 쓴다.
 */
void fillShaderStages(PipelineStates& states, const rhi::GraphicsPipelineDesc& desc) {
    states.stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    states.stages[0].module = toVkHandle<vk::ShaderModule>(desc.vertexShader);
    states.stages[0].pName = "main";

    states.stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    states.stages[1].module = toVkHandle<vk::ShaderModule>(desc.fragmentShader);
    states.stages[1].pName = "main";
}

/**
 * @brief rhi::VertexLayout을 정점 입력 상태로 옮긴다.
 *
 * Phase 0은 binding 하나만 쓴다. 모든 attribute가 같은 binding을 가리킨다.
 *
 * @param[in,out] states 채울 상태.
 * @param[in]     layout 옮길 배치.
 */
void fillVertexInput(PipelineStates& states, const rhi::VertexLayout& layout) {
    states.binding.binding = layout.binding;
    states.binding.stride = layout.stride;
    states.binding.inputRate = vk::VertexInputRate::eVertex;

    states.attributes.resize(layout.attributes.size());
    for (std::size_t index = 0; index < states.attributes.size(); ++index) {
        const rhi::VertexAttribute& source = layout.attributes[index];
        vk::VertexInputAttributeDescription& target = states.attributes[index];
        target.location = source.location;
        target.binding = layout.binding;
        target.format = toVulkan(source.format);
        target.offset = source.offset;
    }

    states.vertexInput.vertexBindingDescriptionCount = 1;
    states.vertexInput.pVertexBindingDescriptions = &states.binding;
    states.vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(states.attributes.size());
    states.vertexInput.pVertexAttributeDescriptions = states.attributes.data();
}

/**
 * @brief 기술자에서 정해지는 나머지 상태를 채운다.
 *
 * 토폴로지와 래스터라이저 상태 말고는 Phase 0에서 값이 하나뿐이라 상수로 둔다.
 * viewport와 scissor는 동적 상태다. 파이프라인에 크기를 박지 않으므로 창 크기가
 * 바뀌어도 다시 만들 필요가 없고, 대신 encoder가 draw 전에 setViewport() 와
 * setScissor()로 값을 넣는다 (frame.md §6).
 *
 * @param[in,out] states 채울 상태.
 * @param[in]     desc   토폴로지와 래스터라이저 상태를 읽을 기술자.
 */
void fillFixedStates(PipelineStates& states, const rhi::GraphicsPipelineDesc& desc) {
    states.inputAssembly.topology = toVulkan(desc.topology);
    states.inputAssembly.primitiveRestartEnable = vk::False;

    states.viewportState.viewportCount = 1;
    states.viewportState.scissorCount = 1;

    states.rasterization.depthClampEnable = vk::False;
    states.rasterization.rasterizerDiscardEnable = vk::False;
    states.rasterization.polygonMode = vk::PolygonMode::eFill;
    states.rasterization.cullMode = toVulkan(desc.rasterizer.cullMode);
    states.rasterization.frontFace = toVulkan(desc.rasterizer.frontFace);
    states.rasterization.depthBiasEnable = vk::False;
    states.rasterization.lineWidth = 1.0f;

    states.multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;
    states.multisample.sampleShadingEnable = vk::False;

    states.blendAttachment.blendEnable = vk::False;
    states.blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    states.colorBlend.logicOpEnable = vk::False;
    states.colorBlend.attachmentCount = 1;
    states.colorBlend.pAttachments = &states.blendAttachment;

    states.dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    states.dynamicState.dynamicStateCount = static_cast<std::uint32_t>(states.dynamicStates.size());
    states.dynamicState.pDynamicStates = states.dynamicStates.data();
}

/**
 * @brief 파이프라인을 만들고 vk 핸들을 돌려준다.
 *
 * 호환용 render pass와 빈 layout은 생성 호출에서만 쓰이므로 여기서 만들고 바로
 * 파괴한다. 파이프라인은 두 핸들을 보관하지 않는다.
 *
 * @param[in] device 파이프라인을 만들 디바이스.
 * @param[in] desc   파이프라인 설정.
 * @return 만들어진 파이프라인.
 */
[[nodiscard]]
vk::Pipeline createGraphicsPipelineHandle(vk::Device device, const rhi::GraphicsPipelineDesc& desc) {
    const vk::RenderPass renderPass = createCompatibleColorRenderPass(device, desc.colorFormat);
    const vk::PipelineLayout layout = createEmptyPipelineLayout(device);

    PipelineStates states;
    fillShaderStages(states, desc);
    fillVertexInput(states, desc.vertexLayout);
    fillFixedStates(states, desc);

    const auto created = device.createGraphicsPipeline(nullptr, states.createInfo(layout, renderPass));

    device.destroyPipelineLayout(layout);
    device.destroyRenderPass(renderPass);

    VK_CHECK(created.result);
    return created.value;
}

} // namespace

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

GraphicsPipeline createGraphicsPipeline(const Device& device, const GraphicsPipelineDesc& desc) {
    NXT_ASSERT_MSG(desc.vertexShader.valid(), "유효하지 않은 vertex shader로 pipeline을 만들려 했다.");
    NXT_ASSERT_MSG(desc.fragmentShader.valid(), "유효하지 않은 fragment shader로 pipeline을 만들려 했다.");
    NXT_ASSERT_MSG(desc.colorFormat != Format::Undefined, "색 부착물의 형식이 정해지지 않았다.");
    NXT_ASSERT_MSG(desc.vertexLayout.stride > 0, "정점 stride가 0인 layout으로 pipeline을 만들 수 없다.");
    NXT_ASSERT_MSG(!desc.vertexLayout.attributes.empty(), "정점 속성이 하나도 없는 layout이다.");

    const vk::Device vkDevice = vulkan::toVulkanDevice(device).device();
    const vk::Pipeline vkPipeline = vulkan::createGraphicsPipelineHandle(vkDevice, desc);

    const GraphicsPipeline pipeline = vulkan::fromVkHandle<GraphicsPipelineTag>(vkPipeline);
    NXT_LOG_DEBUG(Graphics, Vulkan, "graphics pipeline created: pipeline={:#x}", pipeline.value());
    return pipeline;
}

void destroyGraphicsPipeline(const Device& device, const GraphicsPipeline& pipeline) {
    NXT_ASSERT_MSG(pipeline.valid(), "유효하지 않은 graphics pipeline을 해제하려 했다.");

    const vk::Pipeline vkPipeline = vulkan::toVkHandle<vk::Pipeline>(pipeline);
    vulkan::toVulkanDevice(device).device().destroyPipeline(vkPipeline);
    NXT_LOG_DEBUG(Graphics, Vulkan, "graphics pipeline destroyed: pipeline={:#x}", pipeline.value());
}

} // namespace nxt::graphics::rhi
