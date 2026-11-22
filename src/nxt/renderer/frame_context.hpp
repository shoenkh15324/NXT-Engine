#pragma once

#include <nxt/graphics/rhi/rhi.hpp>
#include <span>

namespace nxt::renderer {

/**
 * @brief RHI 타입을 짧게 쓰기 위한 별칭이다.
 *
 * 헤더에서 using namespace를 전개하면 이 헤더를 include하는 모든 TU에 rhi의 이름이
 * 풀린다. 공개 계약인 헤더에서는 별칭 하나만 노출하고 타입은 rhi::로 한정한다.
 */
namespace rhi = ::nxt::graphics::rhi;

struct WorldObject;

/**
 * @brief 한 프레임 동안만 사는 렌더러 쪽 임시 상태이다.
 *
 * 프레임을 넘어 살아남는 것(월드, GPU 핸들, 메시)은 World가 들고, 이번 프레임을 그리는
 * 동안에만 쓰고 버리는 것을 이 구조체가 든다 (scene_world.md §5). Phase 1 1-A4는
 * 컬링 결과인 visible 목록(DrawList의 뷰)까지 들고, 카메라 행렬과 업로드용 임시
 * 버퍼는 앞으로 여기에 붙는다.
 *
 * engine이 넘긴 engine::FrameTarget을 renderer의 어휘로 옮긴 값이다. 의존 방향이
 * engine → renderer라서 renderer는 engine을 include할 수 없고, 그 변환은 두 계층을
 * 잇는 쪽(engine 또는 앱)이 한다.
 *
 * @note 값 타입이다. 매 프레임 스택에 만들고 제출이 끝나면 버린다.
 * @note Thread safety: thread-safe하지 않다. 기록하는 스레드 하나만 쓴다.
 */
struct FrameContext {
    /// @brief 이번 프레임의 색 부착물이다. 보통 swapchain 이미지의 view이다.
    rhi::ImageView color;

    /// @brief color의 픽셀 형식이다. RenderPassDesc::colorFormat과 같은 값이다.
    rhi::Format colorFormat = rhi::Format::Undefined;

    /// @brief 이번 프레임의 깊이 부착물이다. 프레임 루프가 만든 깊이 view이다.
    rhi::ImageView depth;

    /// @brief depth의 픽셀 형식이다. 파이프라인을 만들 때 쓴 값과 같아야 한다.
    rhi::Format depthFormat = rhi::Format::Undefined;

    /// @brief 렌더 영역의 크기이다. 두 부착물의 크기와 같아야 한다.
    rhi::Extent2D extent;

    /**
     * @brief 이번 프레임에 그릴 visible 오브젝트 목록이다.
     *
     * Engine이 buildDrawList()로 채운 DrawList의 뷰를 넘긴다. 비어 있으면
     * clear만 하고 draw는 기록하지 않는다.
     *
     * @note World의 오브젝트를 가리킨다. World를 수정하지 않는 동안, 이번
     *       프레임 동안만 유효하다.
     */
    std::span<const WorldObject* const> drawList{};
};

/**
 * @brief 프레임 임시 상태에 clear 색을 붙여 render pass 기술자로 만든다.
 *
 * 패스는 rhi::RenderPassDesc만 받으므로(rhi.md §10), 어느 부착물에 그릴지 아는 쪽이
 * 이 변환을 한 번만 한다. clear 색은 렌더러의 정책이므로 여기서 정하지 않고 호출자가
 * 넘긴다.
 *
 * @param[in] context    이번 프레임의 부착물과 크기.
 * @param[in] clearColor 색 부착물을 지울 때 채울 색.
 * @return 두 부착물과 크기, clear 색을 채운 기술자. clearDepth는 기본값(1.0)이다.
 */
[[nodiscard]]
rhi::RenderPassDesc makeRenderPassDesc(const FrameContext& context, rhi::ClearColor clearColor) noexcept;

} // namespace nxt::renderer
