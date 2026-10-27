#pragma once

#include <cstdint>
#include <memory>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/core/instance.hpp>
#include <nxt/graphics/rhi/frame/semaphore.hpp>
#include <nxt/graphics/rhi/resource/image.hpp>
#include <optional>

namespace nxt::graphics::rhi {

class Device;

/// @brief 렌더 타겟의 크기이다. 픽셀 단위이다.
struct Extent2D {
    /// @brief 가로 길이.
    std::uint32_t width = 0;

    /// @brief 세로 길이.
    std::uint32_t height = 0;
};

/**
 * @brief Swapchain을 만드는 데 필요한 설정이다.
 *
 * 크기·이미지 개수·픽셀 형식은 넣지 않는다. 전부 Surface의 capabilities에서
 * backend가 고른다 — 프레임 레인 수는 이미지 개수이고(트리플 버퍼링, frame.md §5),
 * 형식은 렌더 패스가 format()으로 같은 값을 조회해 쓴다. 호출자가 정할 것은
 * present 방식뿐이다.
 */
struct SwapchainDesc {
    /// @brief 그릴 대상 창이야. 유효한 Surface여야 한다.
    Surface surface;

    /// @brief 화면에 제시하는 방식. 기본값이 Fifo이다.
    PresentMode presentMode = PresentMode::Fifo;
};

/**
 * @brief 화면에 제시할 이미지들을 순환시키는 객체이다.
 *
 * acquire로 이미지를 하나 얻고, 그 이미지에 그린 뒤, present로 화면에 내보낸다.
 * 이미지와 image view는 swapchain이 소유하며, 창 크기가 바뀌면 함께 다시 만들어진다
 * (frame.md §4, §6).
 *
 * @note 값 핸들이 아니라 추상 클래스인 이유는 vk 핸들 외에 자체 상태를 들고 있기
 *       때문이다 — 픽셀 형식, 이미지 개수, image view 배열 (phase0.md
 *       "RHI 타입은 두 패턴").
 * @note 소유권: 이 객체가 이미지와 view를 소유한다. Device와 Surface보다 먼저
 *       파괴해야 한다 — Surface가 사라지면 그 위에 만든 swapchain도 더 이상
 *       제시할 수 없다 (instance.hpp).
 * @note Thread safety: thread-safe하지 않다. acquire · present · recreate는 같은
 *       스레드에서 순서대로 부른다.
 */
class Swapchain {
public:
    virtual ~Swapchain() = default;

    /**
     * @brief Vulkan swapchain을 만든다.
     *
     * minImageCount는 3(트리플 버퍼링)이고 present mode는 desc가 정한다
     * (frame.md §5). 형식과 크기는 Surface의 capabilities에서 고른다.
     *
     * @param[in] device swapchain을 만들 디바이스. GPU 객체는 디바이스를 첫 인자로
     *                   받는다 (phase0.md A5).
     * @param[in] desc surface와 present 방식.
     * @return 만들어진 swapchain.
     */
    [[nodiscard]]
    static std::unique_ptr<Swapchain> create(const Device& device, const SwapchainDesc& desc);

    /**
     * @brief 다음에 그릴 이미지를 얻는다.
     *
     * 반환된 시점부터 GPU가 그 이미지에 쓸 수 있으므로 present 전까지 다른 곳에서
     * 쓰면 안 된다 (frame.md §4).
     *
     * @param[in] signalSemaphore 이 이미지가 사용 가능해지면 신호할 semaphore.
     *                             submit이 이 semaphore를 기다린다.
     * @return 이미지 인덱스. nullopt면 swapchain이 낡아 recreate()가 필요하다.
     *
     * @note 이 semaphore는 프레임 인덱스로 색인한다. present용 semaphore의
     *       색인 기준(이미지 인덱스)과 다르다 (frame.md §5).
     */
    [[nodiscard]]
    virtual std::optional<std::uint32_t> acquireNextImage(const Semaphore& signalSemaphore) = 0;

    /**
     * @brief 그린 이미지를 화면에 제시한다.
     *
     * @param[in] imageIndex   acquireNextImage()가 돌려준 인덱스.
     * @param[in] waitSemaphore 이 이미지에 대한 GPU 작업이 끝나면 신호되는 semaphore.
     * @return 제시에 성공하면 true. false면 swapchain이 낡아 recreate()가 필요하다.
     *
     * @note present용 semaphore는 이미지 인덱스로 색인한다. vkQueuePresentKHR가
     *       fence를 신호하지 않아 프레임 인덱스로는 안전을 보장할 수 없다
     *       (frame.md §5).
     */
    [[nodiscard]]
    virtual bool present(std::uint32_t imageIndex, const Semaphore& waitSemaphore) = 0;

    /**
     * @brief 지정한 이미지의 view를 반환한다. 렌더 패스의 color attachment로 쓴다.
     *
     * @param[in] imageIndex 0 이상 imageCount() 미만인 인덱스.
     * @return 그 이미지의 view.
     */
    [[nodiscard]]
    virtual ImageView imageView(std::uint32_t imageIndex) const = 0;

    /**
     * @brief 이미지의 픽셀 형식을 반환한다.
     *
     * 렌더 패스의 color attachment 형식은 이 값과 같아야 한다.
     */
    [[nodiscard]]
    virtual Format format() const noexcept = 0;

    /**
     * @brief 이미지 개수를 반환한다.
     *
     * 프레임 레인 수와 present용 semaphore 배열의 크기가 이 값이다 (frame.md §5).
     */
    [[nodiscard]]
    virtual std::uint32_t imageCount() const noexcept = 0;

    /// @brief 현재 이미지의 크기를 반환한다.
    [[nodiscard]]
    virtual Extent2D extent() const noexcept = 0;

    /**
     * @brief 현재 Surface 크기로 swapchain과 image view들을 다시 만든다.
     *
     * 창 크기가 바뀌면 swapchain이 무효가 된다. 호출 전에 in-flight 프레임의
     * fence를 모두 기다려야 한다 — GPU가 아직 쓰는 이미지를 파괴하면
     * use-after-free다 (frame.md §6, §9).
     *
     * @note 크기를 인자로 받지 않는 이유는 Surface의 capabilities(currentExtent)가
     *       이미 창 크기를 알려 주기 때문이다. 크기를 넘기면 호출자마다 창 크기와
     *       swapchain 크기가 어긋날 수 있다.
     */
    virtual void recreate() = 0;
};

} // namespace nxt::graphics::rhi
