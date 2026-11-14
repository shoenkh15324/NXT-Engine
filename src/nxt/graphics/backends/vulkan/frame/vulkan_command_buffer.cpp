#include <array>
#include <cstdint>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_check.hpp>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/core/device.hpp>
#include <nxt/graphics/rhi/frame/command_buffer.hpp>
#include <vector>

namespace nxt::graphics::vulkan {

namespace {

/**
 * @brief 색 부착물과 깊이 부착물을 가진 render pass를 만든다.
 *
 * Phase 0은 단일 render pass다 (rhi.md §10). load op과 store op을 인자로 받지 않는
 * 이유는 값이 하나뿐이어서다 — swapchain 이미지의 내용은 acquire할 때마다 보장되지
 * 않으므로 매 프레임 clear할 수밖에 없고, 제시할 결과는 반드시 남겨야 한다
 * (frame.md §4). 깊이는 그리기 중에만 쓰이므로 clear하고 남기지 않는다.
 *
 * backend의 resource/vulkan_pipeline.cpp에 있는 createCompatibleRenderPass()와 부착물 ·
 * subpass 구성이 같아야 한다. 한쪽만 고치면 그리기에서 pipeline incompatibility가 난다.
 *
 * @param[in] device      render pass를 만들 디바이스.
 * @param[in] colorFormat 색 부착물의 형식. view 핸들로는 조회할 수 없어 호출자가 준다.
 * @param[in] depthFormat 깊이 부착물의 형식. 같은 이유로 호출자가 준다.
 * @return 만들어진 render pass.
 */
[[nodiscard]]
vk::RenderPass createRenderPass(vk::Device device, rhi::Format colorFormat, rhi::Format depthFormat) {
    std::array<vk::AttachmentDescription, 2> attachments{};

    attachments[0].format = toVulkan(colorFormat);
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    // eUndefined는 "이전 내용을 보지 않는다"는 뜻이라 clear와 짝이 맞다.
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    // 이 pass가 마지막이므로 present가 읽을 수 있는 layout으로 끝낸다. 그래야
    // vkQueuePresentKHR 앞에 별도 barrier를 넣지 않아도 된다 (frame.md §8).
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    attachments[1].format = toVulkan(depthFormat);
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
    // 이 pass 밖에서 깊이를 읽지 않는다. 남기지 않아도 다음 프레임이 다시 clear한다.
    attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    vk::RenderPassCreateInfo createInfo{};
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    const auto created = device.createRenderPass(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

/**
 * @brief 색 부착물과 깊이 부착물을 붙인 framebuffer를 만든다.
 *
 * attachment 순서는 render pass의 attachment 순서와 같아야 한다. 0번이 색,
 * 1번이 깊이다.
 *
 * @param[in] device     framebuffer를 만들 디바이스.
 * @param[in] renderPass 이 framebuffer가 쓰는 render pass.
 * @param[in] color      색 부착물로 쓸 view.
 * @param[in] depth      깊이 부착물로 쓸 view.
 * @param[in] extent     framebuffer의 크기.
 * @return 만들어진 framebuffer.
 */
[[nodiscard]]
vk::Framebuffer createFramebuffer(vk::Device device, vk::RenderPass renderPass, vk::ImageView color,
                                  vk::ImageView depth, const rhi::Extent2D& extent) {
    const std::array<vk::ImageView, 2> attachments{color, depth};

    vk::FramebufferCreateInfo createInfo{};
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = extent.width;
    createInfo.height = extent.height;
    createInfo.layers = 1;

    const auto created = device.createFramebuffer(createInfo);
    VK_CHECK(created.result);
    return created.value;
}

/**
 * @brief rhi::CommandEncoder의 Vulkan 구현이다.
 *
 * render pass는 이 객체가 들고 있는다. 부착물 형식만으로 정해지고 image view와 무관하므로
 * swapchain을 재생성해도 그대로 쓸 수 있다. 반면 framebuffer는 특정 view에 묶이는데
 * 그 view는 swapchain이 재생성할 때마다 파괴되므로 캐시하지 않고 pass마다 만든다.
 * 드라이버 호출 두 번은 측정된 병목이 아니다 (CODING_RULES.md §16).
 *
 * @note framebuffer를 endRenderPass()에서 버리지 않는다. 그 시점에는 command buffer가
 *       아직 recording 중이고, Vulkan은 recording 중인 command buffer가 참조하는
 *       framebuffer와 render pass를 파괴하는 것을 허용하지 않는다
 *       (VUID-vkEndCommandBuffer-commandBuffer-00059). 파괴는 다음 beginRenderPass()와
 *       소멸자로 미룬다 — 둘 다 이 레인의 fence가 신호된 뒤에 불린다.
 *
 * @note 이 클래스는 rhi::CommandEncoder::create()만 만든다. 다른 TU가 이름을 알
 *       필요가 없어 헤더를 두지 않는다.
 * @note rhi::CommandEncoder::create()가 이 클래스를 만들어 unique_ptr로 돌려준다.
 *       직접 만들지 않는다.
 */
class VulkanCommandEncoder final : public rhi::CommandEncoder {
public:
    VulkanCommandEncoder(const VulkanDevice& device, vk::CommandBuffer commandBuffer) noexcept
        : device_{device}, commandBuffer_{commandBuffer} {}

    ~VulkanCommandEncoder() override {
        // begin/end 사이에서 encoder가 사라지는 경우까지 정리한다.
        // 프레임 루프가 이 레인의 fence를 기다리고 device.waitIdle()까지 마친 뒤에
        // 부른다 — 그 전에 부르면 실행 중인 명령이 참조하는 render pass를 파괴한다.
        releaseFramebuffer();
        releaseRenderPass();
    }

    VulkanCommandEncoder(const VulkanCommandEncoder&) = delete;
    VulkanCommandEncoder& operator=(const VulkanCommandEncoder&) = delete;

    void beginRenderPass(const rhi::RenderPassDesc& desc) override {
        NXT_ASSERT_MSG(desc.color.valid(), "유효하지 않은 color attachment로 render pass를 시작하려 했다.");
        NXT_ASSERT_MSG(desc.colorFormat != rhi::Format::Undefined, "color attachment의 형식이 정해지지 않았다.");
        NXT_ASSERT_MSG(desc.depth.valid(), "유효하지 않은 depth attachment로 render pass를 시작하려 했다.");
        NXT_ASSERT_MSG(desc.depthFormat != rhi::Format::Undefined, "depth attachment의 형식이 정해지지 않았다.");
        NXT_ASSERT_MSG((desc.extent.width > 0) && (desc.extent.height > 0), "render area의 크기가 0이다.");
        NXT_ASSERT_MSG(!passActive_, "이전 render pass를 끝내지 않고 다시 시작하려 했다.");

        // 지난번 이 레인이 쓴 framebuffer를 여기서 버린다. 프레임 루프가 이 레인의
        // fence를 기다린 뒤 beginCommandBuffer()를 불렀으므로 그 framebuffer를 쓴
        // 제출은 이미 끝났고, 방금 reset된 command buffer는 아무것도 참조하지 않는다.
        // endRenderPass()에서 버리면 recording 중인 command buffer가 참조하는 대상을
        // 파괴하게 된다 (VUID-vkEndCommandBuffer-commandBuffer-00059).
        releaseFramebuffer();

        // render pass는 부착물 형식에만 달려 있다. 형식이 그대로면 pass마다 다시
        // 만들지 않고 재사용한다.
        if (!renderPass_ || (renderPassColorFormat_ != desc.colorFormat) ||
            (renderPassDepthFormat_ != desc.depthFormat)) {
            releaseRenderPass();
            renderPass_ = createRenderPass(device_.device(), desc.colorFormat, desc.depthFormat);
            renderPassColorFormat_ = desc.colorFormat;
            renderPassDepthFormat_ = desc.depthFormat;
        }

        const vk::ImageView color = toVkHandle<vk::ImageView>(desc.color);
        const vk::ImageView depth = toVkHandle<vk::ImageView>(desc.depth);
        framebuffer_ = createFramebuffer(device_.device(), renderPass_, color, depth, desc.extent);

        // 첨부물 순서는 render pass와 같아야 한다. 0번이 색, 1번이 깊이다.
        const std::array<vk::ClearValue, 2> clearValues{
            vk::ClearColorValue{desc.clearColor.red, desc.clearColor.green, desc.clearColor.blue,
                                desc.clearColor.alpha},
            vk::ClearDepthStencilValue{desc.clearDepth, 0},
        };

        vk::RenderPassBeginInfo beginInfo{};
        beginInfo.renderPass = renderPass_;
        beginInfo.framebuffer = framebuffer_;
        beginInfo.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{desc.extent.width, desc.extent.height}};
        beginInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        beginInfo.pClearValues = clearValues.data();

        commandBuffer_.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
        passActive_ = true;
    }

    void endRenderPass() override {
        NXT_ASSERT_MSG(passActive_, "시작하지 않은 render pass를 끝내려 했다.");

        commandBuffer_.endRenderPass();

        // framebuffer는 여기서 파괴하지 않는다. 아직 recording 중이고, recording 중인
        // command buffer가 참조하는 대상을 파괴하면 그 command buffer가 무효가 된다.
        // 파괴는 다음 beginRenderPass()로 미룬다.
        passActive_ = false;
    }

    void setPipeline(const rhi::GraphicsPipeline& pipeline) override {
        NXT_ASSERT_MSG(passActive_, "render pass 밖에서 pipeline을 묶으려 했다.");
        NXT_ASSERT_MSG(pipeline.valid(), "유효하지 않은 pipeline을 묶으려 했다.");

        commandBuffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, toVkHandle<vk::Pipeline>(pipeline));
    }

    void setViewport(const rhi::Extent2D& extent) override {
        NXT_ASSERT_MSG(passActive_, "render pass 밖에서 viewport를 정하려 했다.");
        NXT_ASSERT_MSG((extent.width > 0) && (extent.height > 0), "viewport의 크기가 0이다.");

        // 깊이 범위는 [0, 1]이다. Vulkan의 clip 공간 z와 같다.
        const vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                    0.0f, 1.0f};
        commandBuffer_.setViewport(0, viewport);
    }

    void setScissor(const rhi::Extent2D& extent) override {
        NXT_ASSERT_MSG(passActive_, "render pass 밖에서 scissor를 정하려 했다.");
        NXT_ASSERT_MSG((extent.width > 0) && (extent.height > 0), "scissor의 크기가 0이다.");

        commandBuffer_.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, vk::Extent2D{extent.width, extent.height}});
    }

    void setVertexBuffer(std::uint32_t binding, const rhi::Buffer& buffer) override {
        NXT_ASSERT_MSG(passActive_, "render pass 밖에서 vertex buffer를 묶으려 했다.");
        NXT_ASSERT_MSG(buffer.valid(), "유효하지 않은 vertex buffer를 묶으려 했다.");

        // Phase 0은 버퍼 앞에서부터 그리므로 오프셋은 0이다.
        const std::array<vk::Buffer, 1> buffers{toVkHandle<vk::Buffer>(buffer)};
        const std::array<vk::DeviceSize, 1> offsets{0};
        commandBuffer_.bindVertexBuffers(binding, buffers, offsets);
    }

    void draw(std::uint32_t vertexCount, std::uint32_t instanceCount, std::uint32_t firstVertex,
              std::uint32_t firstInstance) override {
        NXT_ASSERT_MSG(passActive_, "render pass 밖에서 draw를 기록하려 했다.");
        NXT_ASSERT_MSG(vertexCount > 0, "정점 0개를 그리려 했다.");

        commandBuffer_.draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

private:
    /// @brief framebuffer를 파괴한다. 다음 pass가 시작되거나 encoder가 사라질 때 부른다.
    void releaseFramebuffer() noexcept {
        if (framebuffer_) {
            device_.device().destroyFramebuffer(framebuffer_);
            framebuffer_ = vk::Framebuffer{};
        }
    }

    /// @brief render pass를 파괴한다. 부착물 형식이 바뀌거나 encoder가 사라질 때 부른다.
    void releaseRenderPass() noexcept {
        if (renderPass_) {
            device_.device().destroyRenderPass(renderPass_);
            renderPass_ = vk::RenderPass{};
            renderPassColorFormat_ = rhi::Format::Undefined;
            renderPassDepthFormat_ = rhi::Format::Undefined;
        }
    }

    /// @brief command buffer를 만든 디바이스. 이 객체보다 오래 산다.
    const VulkanDevice& device_;

    /// @brief 기록할 대상이다. 소유하지 않는다.
    vk::CommandBuffer commandBuffer_{};

    /// @brief beginRenderPass()가 만들고 다음 beginRenderPass()가 파괴하는 framebuffer.
    vk::Framebuffer framebuffer_{};

    /// @brief 이 encoder가 쓰는 render pass. 부착물 형식이 정해진 뒤에만 유효하다.
    vk::RenderPass renderPass_{};

    /// @brief renderPass_를 만든 색 부착물 형식. 다르면 다시 만든다.
    rhi::Format renderPassColorFormat_{rhi::Format::Undefined};

    /// @brief renderPass_를 만든 깊이 부착물 형식. 다르면 다시 만든다.
    rhi::Format renderPassDepthFormat_{rhi::Format::Undefined};

    /// @brief beginRenderPass()와 endRenderPass()가 짝을 이루는지 확인한다.
    bool passActive_ = false;
};

} // namespace

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

CommandBuffer createCommandBuffer(const Device& device) {
    const vulkan::VulkanDevice& vulkanDevice = vulkan::toVulkanDevice(device);

    vk::CommandBufferAllocateInfo allocateInfo{};
    allocateInfo.commandPool = vulkanDevice.commandPool();
    allocateInfo.level = vk::CommandBufferLevel::ePrimary;
    allocateInfo.commandBufferCount = 1;

    const auto allocated = vulkanDevice.device().allocateCommandBuffers(allocateInfo);
    VK_CHECK(allocated.result);
    NXT_ASSERT_MSG(allocated.value.size() == 1, "command buffer를 하나 요청했는데 다른 수가 돌아왔다.");

    const CommandBuffer commandBuffer = vulkan::fromVkHandle<CommandBufferTag>(allocated.value.front());
    NXT_LOG_DEBUG(Graphics, Vulkan, "command buffer created: commandBuffer={:#x}", commandBuffer.value());
    return commandBuffer;
}

void destroyCommandBuffer(const Device& device, const CommandBuffer& commandBuffer) {
    NXT_ASSERT_MSG(commandBuffer.valid(), "유효하지 않은 command buffer를 해제하려 했다.");

    const vulkan::VulkanDevice& vulkanDevice = vulkan::toVulkanDevice(device);
    vulkanDevice.device().freeCommandBuffers(vulkanDevice.commandPool(),
                                             vulkan::toVkHandle<vk::CommandBuffer>(commandBuffer));
    NXT_LOG_DEBUG(Graphics, Vulkan, "command buffer destroyed: commandBuffer={:#x}", commandBuffer.value());
}

void beginCommandBuffer(const Device& device, const CommandBuffer& commandBuffer) {
    (void)device; // vkBeginCommandBuffer는 커맨드 버퍼만 받는다. 시그니처는 A5 규칙대로 둔다.
    NXT_ASSERT_MSG(commandBuffer.valid(), "유효하지 않은 command buffer의 기록을 시작하려 했다.");

    const vk::CommandBuffer vkCommandBuffer = vulkan::toVkHandle<vk::CommandBuffer>(commandBuffer);

    // 매 프레임 같은 버퍼를 다시 기록하므로 먼저 되돌린다 (frame.md §5). 디바이스의
    // pool이 eResetCommandBuffer로 만들어져 있어 버퍼 단위 reset이 허용된다.
    VK_CHECK(vkCommandBuffer.reset());

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    VK_CHECK(vkCommandBuffer.begin(beginInfo));
}

void endCommandBuffer(const Device& device, const CommandBuffer& commandBuffer) {
    (void)device; // end도 커맨드 버퍼만 받는다. start와 같은 이유로 시그니처만 맞춘다.
    NXT_ASSERT_MSG(commandBuffer.valid(), "유효하지 않은 command buffer의 기록을 닫으려 했다.");

    const vk::CommandBuffer vkCommandBuffer = vulkan::toVkHandle<vk::CommandBuffer>(commandBuffer);
    VK_CHECK(vkCommandBuffer.end());
}

std::unique_ptr<CommandEncoder> CommandEncoder::create(const Device& device, const CommandBuffer& commandBuffer) {
    NXT_ASSERT_MSG(commandBuffer.valid(), "유효하지 않은 command buffer로 encoder를 만들려 했다.");

    const vulkan::VulkanDevice& vulkanDevice = vulkan::toVulkanDevice(device);
    const vk::CommandBuffer vkCommandBuffer = vulkan::toVkHandle<vk::CommandBuffer>(commandBuffer);

    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다 (VulkanSwapchain과 같은 이유).
    return std::unique_ptr<CommandEncoder>(new vulkan::VulkanCommandEncoder(vulkanDevice, vkCommandBuffer));
}

void submit(const Device& device, const CommandBuffer& commandBuffer, const SubmitDesc& desc) {
    NXT_ASSERT_MSG(commandBuffer.valid(), "유효하지 않은 command buffer를 제출하려 했다.");
    NXT_ASSERT_MSG(desc.waitSemaphore.valid(), "제출에 기다릴 semaphore가 없다.");
    NXT_ASSERT_MSG(desc.signalSemaphore.valid(), "제출이 신호할 semaphore가 없다.");
    NXT_ASSERT_MSG(desc.fence.valid(), "제출이 신호할 fence가 없다.");

    const vulkan::VulkanDevice& vulkanDevice = vulkan::toVulkanDevice(device);

    const vk::CommandBuffer vkCommandBuffer = vulkan::toVkHandle<vk::CommandBuffer>(commandBuffer);
    const vk::Semaphore wait = vulkan::toVkHandle<vk::Semaphore>(desc.waitSemaphore);
    const vk::Semaphore signal = vulkan::toVkHandle<vk::Semaphore>(desc.signalSemaphore);
    const vk::Fence fence = vulkan::toVkHandle<vk::Fence>(desc.fence);

    // acquire semaphore는 색 부착물에 쓰기 시작하기 전에만 기다리면 된다. 그래서
    // 기다리는 지점을 ColorAttachmentOutput으로 둔다 (frame.md §8의 hand-off barrier).
    const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &wait;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signal;

    const vk::Queue queue = vulkan::toVkHandle<vk::Queue>(vulkanDevice.graphicsQueue());

    // Hpp의 편의 오버로드는 실패를 assert로만 남긴다. VULKAN_HPP_NO_EXCEPTIONS에서
    // Release에 사라지므로 반환된 Result를 여기서 검사한다 (rhi.md §7).
    VK_CHECK(queue.submit(submitInfo, fence));
}

} // namespace nxt::graphics::rhi
