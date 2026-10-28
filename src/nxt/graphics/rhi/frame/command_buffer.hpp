#pragma once

#include <memory>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/frame/fence.hpp>
#include <nxt/graphics/rhi/frame/semaphore.hpp>
#include <nxt/graphics/rhi/frame/swapchain.hpp>
#include <nxt/graphics/rhi/resource/image.hpp>

namespace nxt::graphics::rhi {

class Device;

/// @brief CommandBuffer를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct CommandBufferTag {};

/**
 * @brief GPU에 올릴 명령 목록이다.
 *
 * beginCommandBuffer()로 기록을 열고, 그 사이에 그릴 내용을 기록한 뒤,
 * endCommandBuffer()로 닫고 submit()으로 큐에 올린다 (rhi.md §8).
 *
 * 프레임 레인마다 하나씩 둔다. 프레임 레인 수가 swapchain 이미지 개수이므로
 * (frame.md §5) 그 수만큼 만들고 매 프레임 같은 것을 다시 기록한다.
 *
 * @note 값 핸들이다. 프레임 레인 수만큼 배열로 들고 다니는 쪽이 unique_ptr 배열보다
 *       단순하다 (phase0.md "RHI 타입은 두 패턴"). 커맨드 버퍼를 꺼내는 command
 *       pool은 디바이스가 하나 소유한다 — 스레드도 큐도 하나뿐이라 pool을 나눌
 *       이유가 없다 (rhi.md §8).
 * @note 소유권이 없다. 해제는 destroyCommandBuffer()가 하고, GPU가 아직 읽는 중이면
 *       안 되므로 프레임 경계에서 부른다 (rhi.md §9).
 */
using CommandBuffer = ResourceHandle<CommandBufferTag>;

/**
 * @brief attachment를 clear할 때 채우는 색이다.
 *
 * 성분 범위는 [0, 1]이다. Vulkan의 VkClearColorValue와 같은 float 표기다.
 */
struct ClearColor {
    /// @brief 빨강 성분.
    float red = 0.0f;

    /// @brief 초록 성분.
    float green = 0.0f;

    /// @brief 파랑 성분.
    float blue = 0.0f;

    /// @brief 불투명도. 1이면 불투명하다.
    float alpha = 1.0f;
};

/**
 * @brief render pass 하나를 기록하는 데 필요한 정보다.
 *
 * RHI는 이 기술자를 Vulkan으로 번역할 뿐 무엇을 그리는지 모른다 (rhi.md §10).
 * 그래서 그릴 대상과 크기, clear 색만 받는다.
 *
 * @note Phase 0은 단일 render pass이고 색 부착물 하나를 매 프레임 clear한다 —
 *       swapchain 이미지의 내용은 acquire할 때마다 보장되지 않기 때문이다
 *       (frame.md §4). 깊이 부착물은 B4에서 더한다.
 */
struct RenderPassDesc {
    /// @brief 색 부착물로 쓸 대상이다. 보통 swapchain.imageView(imageIndex)다.
    ImageView color;

    /**
     * @brief 색 부착물의 픽셀 형식이다.
     *
     * render pass를 만들려면 형식이 필요하다. view 핸들만으로는 형식을 조회할 수
     * 없어 호출자가 넘긴다 — swapchain.format()이 그 값이다.
     */
    Format colorFormat = Format::Undefined;

    /**
     * @brief renderArea이자 framebuffer의 크기다.
     *
     * 대상 이미지의 실제 크기와 같아야 한다. view 핸들만으로는 크기를 조회할 수
     * 없어 호출자가 넘긴다 — swapchain.extent()가 그 값이다.
     */
    Extent2D extent;

    /// @brief 색 부착물을 지울 때 채울 색.
    ClearColor clearColor{};
};

/**
 * @brief 기록할 command buffer를 만든다.
 *
 * Vulkan은 command buffer를 pool에서 꺼낸다. pool은 디바이스가 소유하므로 (rhi.md §8)
 * 이 함수는 버퍼 하나를 꺼내기만 하고, destroyCommandBuffer()는 그 버퍼를 pool로
 * 돌려준다.
 *
 * @param[in] device command buffer를 만들 디바이스.
 * @return 만들어진 command buffer. 프레임 레인 수만큼 만든다.
 */
[[nodiscard]]
CommandBuffer createCommandBuffer(const Device& device);

/**
 * @brief command buffer를 pool로 돌려준다.
 *
 * @param[in] device         command buffer를 만든 디바이스.
 * @param[in] commandBuffer  해제할 command buffer.
 */
void destroyCommandBuffer(const Device& device, const CommandBuffer& commandBuffer);

/**
 * @brief 기록을 시작한다.
 *
 * 이전 기록은 버리고 처음부터 다시 쓴다 — 매 프레임 같은 command buffer를
 * 재사용하기 때문이다 (frame.md §5).
 *
 * @note 그 레인의 이전 제출이 GPU에서 아직 실행 중이면 안 된다. 기록 전에 그 레인의
 *       fence를 기다려야 한다 — 그 순서가 백프레시다 (frame.md §8).
 *
 * @param[in] device        command buffer를 만든 디바이스.
 * @param[in] commandBuffer 기록할 command buffer.
 */
void beginCommandBuffer(const Device& device, const CommandBuffer& commandBuffer);

/**
 * @brief 기록을 닫는다. 닫은 뒤에만 submit()할 수 있다.
 *
 * @param[in] device        command buffer를 만든 디바이스.
 * @param[in] commandBuffer 기록을 닫을 command buffer.
 */
void endCommandBuffer(const Device& device, const CommandBuffer& commandBuffer);

/**
 * @brief 기록 중인 command buffer에 그릴 내용을 넣는 창구이다.
 *
 * renderer는 Device도 CommandBuffer도 받지 않고 이 객체만 받는다
 * (frame.md §2 renderer.record(encoder, target)). 그래서 "renderer는 GPU 상태를
 * 직접 다루지 않는다"는 경계가 관례가 아니라 타입으로 강제된다 (frame.md §3).
 *
 * beginCommandBuffer()와 endCommandBuffer() 사이에서만 유효하다.
 *
 * render pass는 형식만으로 정해지므로 encoder가 들고 있다가 재사용하고, framebuffer는
 * view에 묶이므로 pass마다 만들고 버린다. 그 상태와 해제 순서 때문에 Swapchain과
 * 같이 추상 클래스가 되었다 (phase0.md "RHI 타입은 두 패턴").
 *
 * @note 소유권: CommandEncoder::create()가 만들고 unique_ptr이 파괴한다.
 * @note Thread safety: thread-safe하지 않다. command buffer와 같은 스레드에서 쓴다.
 */
class CommandEncoder {
public:
    /**
     * @brief 기록 중인 command buffer에 붙는 encoder를 만든다.
     *
     * @param[in] device        command buffer를 만든 디바이스.
     * @param[in] commandBuffer beginCommandBuffer()로 열어 둔 command buffer.
     * @return 만들어진 encoder.
     */
    [[nodiscard]]
    static std::unique_ptr<CommandEncoder> create(const Device& device, const CommandBuffer& commandBuffer);

    virtual ~CommandEncoder() = default;

    CommandEncoder(const CommandEncoder&) = delete;
    CommandEncoder& operator=(const CommandEncoder&) = delete;

    /// @brief render pass를 시작한다. 이후 기록은 이 pass에 들어간다.
    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;

    /// @brief render pass를 끝낸다. Phase 0에서 command buffer 하나에 pass가 하나다.
    virtual void endRenderPass() = 0;

protected:
    /**
     * @brief backend 파생 클래스만 만들 수 있다.
     *
     * 복사 생성자를 지웠으므로 기본 생성자도 함께 사라진다. 파생 클래스가 기반을
     * 만들 수 있도록 여기서 되살린다.
     */
    CommandEncoder() = default;
};

/**
 * @brief submit()이 기다리고 신호할 동기화 객체들이다.
 *
 * 두 semaphore의 색인 체계가 다르다. waitSemaphore는 프레임 인덱스로,
 * signalSemaphore는 swapchain 이미지 인덱스로 고른다 (frame.md §5). 지정 초기화로
 * 넘기면 어느 배열에서 꺼낸 값인지 호출부에 드러나 실수를 줄인다.
 */
struct SubmitDesc {
    /// @brief acquire가 신호하는 semaphore. 이 제출은 그것을 기다린다.
    Semaphore waitSemaphore;

    /// @brief 제출이 끝나면 신호할 semaphore. present가 그것을 기다린다.
    Semaphore signalSemaphore;

    /// @brief 제출이 끝나면 신호할 fence. CPU는 다음 프레임에 이 fence를 기다린다.
    Fence fence;
};

/**
 * @brief 기록한 command buffer를 큐에 올린다.
 *
 * 제출은 waitSemaphore를 기다렸다가 시작하고, 끝나면 signalSemaphore와 fence를
 * 신호한다. fence가 백프레시의 근거다 — 다음 프레임이 이 fence를 기다리면 GPU보다
 * 앞서가지 못하고, 지연 파괴도 이 신호를 "더는 쓰지 않는다"의 증거로 쓴다
 * (frame.md §5, §9).
 *
 * @param[in] device        command buffer를 만든 디바이스.
 * @param[in] commandBuffer 기록을 닫은 command buffer.
 * @param[in] desc          기다릴 semaphore, 신호할 semaphore와 fence.
 *
 * @note 레이아웃 전이는 호출자가 하지 않는다. render pass가 initialLayout과
 *       finalLayout으로 처리하므로 제출 앞뒤에 barrier를 넣을 필요가 없다
 *       (frame.md §8).
 */
void submit(const Device& device, const CommandBuffer& commandBuffer, const SubmitDesc& desc);

} // namespace nxt::graphics::rhi
