#pragma once

#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/common/types.hpp>
#include <nxt/graphics/rhi/resource/memory.hpp>

namespace nxt::graphics::rhi {

class Device;

/// @brief Image를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct ImageTag {};

/**
 * @brief GPU가 읽고 쓰는 2차원 이미지이다.
 *
 * 색 부착물, 깊이 버퍼, 셰이더 입력의 원본이 된다. VkImage도 메모리를 갖지 않으므로
 * 그것만 만들어서는 쓸 수 없다. createImage()가 요구 사항을 조회해 메모리까지
 * 할당하고 묶는다. 렌더 타겟이나 셰이더 입력으로 쓰려면 ImageView가 더 필요하다.
 *
 * Phase 0은 2차원 · 밉 1단계 · 배열 1겹만 만든다. 그 이상이 필요해지는 시점에
 * ImageDesc가 넓어진다.
 *
 * @note 값 핸들이다. vk 핸들 외에 자체 상태가 없다 (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권이 없다. createImage()가 만들고 destroyImage()가 해제한다.
 */
using Image = ResourceHandle<ImageTag>;

/// @brief 이미지 뷰를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct ImageViewTag {};

/**
 * @brief 이미지를 렌더 타겟이나 셰이더 입력으로 쓸 수 있게 한 뷰 핸들이다.
 *
 * VkImage는 그저 메모리 블록이라 렌더 타겟으로 직접 쓸 수 없다.
 * vkCreateImageView로 만들어야 하며 그 결과가 이 핸들이다. 스왑체인이 이미지
 * 하나당 하나씩 만들어 주고, 깊이 버퍼는 createImageView()로 만든다.
 *
 * @note 소유권이 없다. 파괴는 backend가 결정한 시점에 일어난다 (rhi.md §9).
 */
using ImageView = ResourceHandle<ImageViewTag>;

/**
 * @brief image를 만드는 데 필요한 설정이다.
 *
 * 크기와 형식 · 용도는 호출자가 정하고, 메모리 성질만 고르면 나머지는 backend가
 * 정한다. 메모리 크기와 type은 드라이버가 요구 사항으로 알려 주므로 여기에 넣지
 * 않는다.
 */
struct ImageDesc {
    /// @brief 이미지의 픽셀 크기이다. 어느 쪽도 0이면 안 된다.
    Extent2D extent;

    /// @brief 픽셀 형식이다. Undefined면 안 된다.
    Format format = Format::Undefined;

    /// @brief 이미지를 어떤 용도로 쓸지 나타낸다. 하나 이상 켜야 한다.
    ImageUsage usage = ImageUsage::None;

    /**
     * @brief 이미지가 묶일 메모리의 성질이다.
     *
     * 기본값 DeviceLocal은 GPU만 접근하는 메모리다. 깊이 버퍼는 CPU가 읽지 않으므로
     * 이것으로 충분하다. CPU가 픽셀을 채우는 이미지는 HostVisible이 필요하지만, 그
     * 경로는 스테이징 버퍼에서 복사하는 쪽을 먼저 넣는다 (phase0.md B4).
     */
    MemoryProperty memoryProperty = MemoryProperty::DeviceLocal;
};

/**
 * @brief 만들어진 이미지와 그 이미지가 묶인 메모리이다.
 *
 * 둘은 따로 만들 수 없다. 이미지의 요구 사항을 알아야 메모리 크기와 type을 정할 수
 * 있고, 그 값은 backend만 조회할 수 있기 때문이다. 그래서 생성과 파괴를 한 쌍으로
 * 묶었다.
 *
 * @note destroyImage()가 이미지를 먼저 지우고 메모리를 해제한다. 순서를 호출자에게
 *       맡기지 않는다.
 */
struct ImageAllocation {
    /// @brief 만들어진 이미지이다.
    Image image;

    /// @brief 그 이미지가 묶인 메모리이다.
    Memory memory;
};

/**
 * @brief image를 만들고 메모리를 할당해 묶는다.
 *
 * 크기와 memory type은 드라이버가 정한다. 정렬 때문에 요구 크기가 픽셀 크기에서
 * 계산한 값보다 클 수 있으므로 할당은 요구 사항의 크기로 한다.
 *
 * @param[in] device image를 만들 디바이스.
 * @param[in] desc   크기 · 형식 · 용도 · 메모리 성질.
 *
 * @return 만들어진 이미지와 메모리.
 */
[[nodiscard]]
ImageAllocation createImage(const Device& device, const ImageDesc& desc);

/**
 * @brief image를 지우고 그 메모리를 해제한다.
 *
 * 이미지를 먼저 지운다. 메모리를 먼저 해제하면 이미지가 죽은 메모리를 가리킨다.
 * 이 이미지에 만든 image view가 남아 있으면 안 된다 — view를 먼저 지운다.
 *
 * @note GPU가 아직 이 이미지를 읽고 있을 수 있다. 프레임 경계에서 부르거나
 *       resource_deletion의 지연 파괴 경로를 쓴다 (frame.md §9).
 *
 * @param[in] device     image를 만든 디바이스.
 * @param[in] allocation 해제할 이미지와 메모리.
 */
void destroyImage(const Device& device, const ImageAllocation& allocation);

/**
 * @brief image view를 만드는 데 필요한 설정이다.
 *
 * Phase 0은 2차원 · 밉 0단계 · 배열 0겹만 본다. 그래서 남은 값은 대상과 형식뿐이다.
 */
struct ImageViewDesc {
    /// @brief 뷰가 가리킬 이미지이다. 유효해야 한다.
    Image image;

    /// @brief 이미지의 픽셀 형식이다. image를 만든 형식과 같아야 한다.
    Format format = Format::Undefined;
};

/**
 * @brief image 위에 view를 만든다.
 *
 * view의 aspect는 형식에서 유도한다. 깊이 형식은 depth, 깊이·스텐실 결합 형식은
 * depth와 stencil, 그 밖의 형식은 color다. 호출자가 따로 고르지 않으므로 깊이
 * 이미지에 color aspect를 주는 실수가 생기지 않는다.
 *
 * @param[in] device view를 만들 디바이스.
 * @param[in] desc   대상 이미지와 형식.
 *
 * @return 만들어진 image view.
 */
[[nodiscard]]
ImageView createImageView(const Device& device, const ImageViewDesc& desc);

/**
 * @brief image view를 해제한다.
 *
 * 이 view를 framebuffer가 참조하고 있으면 안 된다. recording 중인 command buffer와
 * framebuffer는 view보다 먼저 사라져야 한다 (frame.md §6).
 *
 * @param[in] device view를 만든 디바이스.
 * @param[in] view   해제할 view. swapchain이 소유한 view는 넘기지 않는다.
 */
void destroyImageView(const Device& device, const ImageView& view);

} // namespace nxt::graphics::rhi
