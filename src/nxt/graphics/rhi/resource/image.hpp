#pragma once

#include <nxt/graphics/rhi/common/handle.hpp>

namespace nxt::graphics::rhi {

/// @brief 이미지 뷰를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct ImageViewTag {};

/**
 * @brief 이미지를 렌더 타겟이나 셰이더 입력으로 쓸 수 있게 한 뷰 핸들이다.
 *
 * VkImage는 그저 메모리 블록이라 렌더 타겟으로 직접 쓸 수 없다.
 * vkCreateImageView로 만들어야 하며 그 결과가 이 핸들이다. 스왑체인이 이미지
 * 하나당 하나씩 만들어 준다.
 *
 * @note 소유권이 없다. 파괴는 backend가 결정한 시점에 일어난다 (rhi.md §9).
 */
using ImageView = ResourceHandle<ImageViewTag>;

} // namespace nxt::graphics::rhi
