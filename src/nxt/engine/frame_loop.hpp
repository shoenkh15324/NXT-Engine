#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <nxt/graphics/rhi/rhi.hpp>
#include <vector>

namespace nxt::engine {

/**
 * @brief RHI 타입을 짧게 쓰기 위한 별칭이다.
 *
 * 헤더에서 using namespace를 전개하면 이 헤더를 include하는 모든 TU에 rhi의 이름이
 * 풀린다. 공개 계약인 헤더에서는 이름 하나만 노출하고 타입은 rhi::로 한정한다.
 */
namespace rhi = ::nxt::graphics::rhi;

/**
 * @brief 한 프레임에 그릴 대상이다.
 *
 * renderer는 swapchain을 모른다. 그래서 프레임 루프가 swapchain에서 꺼낸 값을 이
 * 구조체로 넘긴다 (frame.md §3). 값 핸들이므로 복사 비용이 없다.
 */
struct FrameTarget {
    /// @brief 이번 프레임의 색 부착물이다. swapchain 이미지의 view이다.
    rhi::ImageView color;

    /// @brief color의 픽셀 형식이다. swapchain.format()이 준 값이다.
    rhi::Format colorFormat = rhi::Format::Undefined;

    /// @brief 렌더 영역의 크기다. swapchain.extent()가 준 값이다.
    rhi::Extent2D extent;

    /// @brief 지금 사용 중인 프레임 레인 인덱스다. 지연 파괴에 이 값을 넣는다.
    std::uint32_t frameIndex = 0;
};

/**
 * @brief 한 프레임에 그릴 내용을 기록하는 함수다.
 *
 * A6 시점에는 renderer가 없으므로 함수로 받는다. 0-C에서 renderer::Renderer가 생기면
 * 그 record()를 감싼 함수를 넘긴다 — 프레임 루프는 무엇을 그리는지 몰라도 된다.
 *
 * @note encoder만 받는다. Device와 CommandBuffer를 직접 주면 renderer가 GPU 상태를
 *       만질 수 있게 되고, "렌더러는 Vulkan을 모른다"는 경계가 무너진다 (frame.md §3).
 */
using RecordFunction = std::function<void(rhi::CommandEncoder& encoder, const FrameTarget& target)>;

/**
 * @brief acquire → record → submit → present를 매 프레임 실행하는 루프다.
 *
 * "무엇을 그릴지"는 모르고 "언제"만 정한다 (frame.md §2). 프레임 레인 수는 swapchain
 * 이미지 개수이고, 레인마다 fence·command buffer·acquire semaphore를 하나씩 둔다.
 * present semaphore는 프레임이 아니라 **이미지 인덱스**로 색인하므로 이미지 개수만큼
 * 따로 둔다 (frame.md §5).
 *
 * @note create()가 만들고 unique_ptr이 파괴한다. 직접 만들지 않는다.
 * @note 소유권: 이 객체는 프레임 레인 자원과 지연 파괴 목록을 소유한다. Device와
 *       Swapchain은 빌려 쓴다 — 둘 다 이 객체보다 오래 살아야 한다.
 * @note Thread safety: thread-safe하지 않다. 프레임 루프는 하나의 스레드에서 돈다.
 */
class FrameLoop final {
public:
    /// @brief frame()이 돌려주는 결과다.
    enum class Status : std::uint8_t {
        /// @brief 제출과 제시까지 끝났다.
        Presented,

        /// @brief swapchain이 낡았다. recreate()를 부른 뒤 다음 프레임에 다시 시도한다.
        OutOfDate,
    };

    /**
     * @brief 프레임 레인을 만들고 루프를 준비한다.
     *
     * @param[in] device    프레임 레인 자원을 만들 디바이스.
     * @param[in] swapchain 제시할 대상. 창 크기가 바뀌면 recreate()로 함께 갱신한다.
     * @param[in] record    매 프레임 그릴 내용을 기록하는 함수. 비어 있으면 안 된다.
     * @return 만들어진 프레임 루프.
     */
    [[nodiscard]]
    static std::unique_ptr<FrameLoop> create(const rhi::Device& device, rhi::Swapchain& swapchain,
                                             RecordFunction record);

    /// @brief 남은 지연 파괴를 비우고 프레임 레인을 파괴한다. GPU 사용이 끝난 뒤여야 한다.
    ~FrameLoop();

    FrameLoop(const FrameLoop&) = delete;
    FrameLoop& operator=(const FrameLoop&) = delete;

    /**
     * @brief 프레임 하나를 실행한다.
     *
     * 순서는 고정이다 (frame.md §4, §5, §9).
     *
     *   1. 이 레인의 fence를 기다린다 (백프레시)
     *   2. 그 프레임이 마지막으로 쓴 리소스를 실제로 파괴한다
     *   3. acquire로 이미지를 하나 얻는다
     *   4. 그 이미지에 그릴 내용을 기록한다
     *   5. 제출하고 화면에 제시한다
     *
     * @return Presented면 다음 프레임을 바로 실행해도 된다. OutOfDate면 recreate()를
     *         부른 뒤 다시 부른다.
     *
     * @note 최소화 상태에서는 acquire가 계속 실패한다. 창에 그릴 영역이 없으면 이
     *       함수를 부르지 않는다.
     */
    [[nodiscard]]
    Status frame();

    /**
     * @brief 창 크기에 맞춰 swapchain을 다시 만들고 프레임 레인을 재조정한다.
     *
     * 리사이즈 순서를 이 함수가 지킨다 (frame.md §6): in-flight fence를 모두 기다린 뒤
     * 지연 파괴를 비우고, swapchain을 재생성한다. 이미지 개수가 바뀌었으면 fence와
     * semaphore, command buffer도 새 개수로 다시 만든다.
     *
     * @note 최소화처럼 그릴 영역이 없으면 swapchain이 바뀌지 않으므로 레인도 그대로 둔다.
     */
    void recreate();

    /**
     * @brief 지연 파괴 목록을 반환한다. renderer가 리소스를 맡길 때 쓴다.
     *
     * frameIndex와 함께 넘기면 이 루프가 그 프레임의 fence 신호 뒤에 파괴한다
     * (frame.md §9).
     */
    [[nodiscard]]
    rhi::ResourceDeletion& deletion() noexcept;

private:
    FrameLoop(const rhi::Device& device, rhi::Swapchain& swapchain, RecordFunction record);

    /// @brief 이미지 개수만큼 프레임 레인을 만든다.
    void createFrames();

    /// @brief 프레임 레인을 모두 파괴한다.
    void destroyFrames();

    /// @brief in-flight fence를 모두 기다린다. 리사이즈와 종료 경로가 쓴다.
    void waitAllFences();

    /// @brief 프레임 레인 자원을 만드는 디바이스. 이 객체보다 오래 산다.
    const rhi::Device& device_;

    /// @brief 제시할 대상. 이 객체보다 오래 산다.
    rhi::Swapchain& swapchain_;

    /// @brief 매 프레임 그릴 내용을 기록하는 함수.
    RecordFunction record_;

    /// @brief 지연 파괴 목록. 이 객체가 소유한다.
    std::unique_ptr<rhi::ResourceDeletion> deletion_;

    /// @brief 레인마다 하나씩 있는 command buffer. 매 프레임 다시 기록한다.
    std::vector<rhi::CommandBuffer> commandBuffers_;

    /// @brief 레인마다 하나씩 있는 fence. 백프레시와 지연 파괴의 근거다.
    std::vector<rhi::Fence> fences_;

    /// @brief 레인마다 하나씩 있는 acquire semaphore. 프레임 인덱스로 색인한다.
    std::vector<rhi::Semaphore> acquireSemaphores_;

    /// @brief 이미지마다 하나씩 있는 present semaphore. 이미지 인덱스로 색인한다.
    std::vector<rhi::Semaphore> presentSemaphores_;

    /// @brief 현재 프레임 레인 수. swapchain 이미지 개수와 같다.
    std::uint32_t imageCount_ = 0;

    /// @brief 지금 사용할 레인. frame()이 끝나면 다음 레인으로 넘어간다.
    std::uint32_t frameIndex_ = 0;
};

} // namespace nxt::engine
