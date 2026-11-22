#pragma once

#include <cstddef>
#include <cstdint>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/renderer/frame_context.hpp>
#include <nxt/renderer/passes/triangle_pass.hpp>
#include <nxt/renderer/world/world.hpp>

namespace nxt::renderer {

namespace rhi = ::nxt::graphics::rhi;

/**
 * @brief 한 프레임에 그릴 것을 조립해 encoder에 기록한다.
 *
 * 패스를 나열하고, 이번 프레임의 부착물에 맞춰 각 패스를 기록한다 (frame.md §3).
 * acquire · submit · present · 동기화는 모른다 — 그 순서는 engine의 프레임 루프가
 * 정하고, renderer는 "무엇을"만 정한다.
 *
 * Phase 1 1-A1은 World의 오브젝트를 삼각형 패스 하나로 그린다. Renderer는 World를
 * 소유하지 않고 매 프레임 참조로 받는다. 셰이더 · 파이프라인 · 정점 버퍼 같은
 * 삼각형에 대한 지식은 TrianglePass가 소유한다.
 *
 * @note 수명을 가진 객체다. 디바이스보다 먼저 만들어지고 디바이스보다 먼저
 *       파괴되어야 한다.
 * @note Thread safety: thread-safe하지 않다. 렌더 스레드 하나에서만 쓴다.
 */
class Renderer {
public:
    /**
     * @brief 패스를 만들고 그릴 준비를 한다.
     *
     * 파이프라인은 부착물의 형식을 알아야 하므로 두 형식을 받는다. 값은
     * FrameContext::colorFormat · depthFormat과 같아야 하고, 다르면 검증 레이어가
     * draw 시점에 거부한다.
     *
     * @param[in] device         패스를 만들 디바이스. 이 객체보다 오래 산다.
     * @param[in] colorFormat    색 부착물의 형식. 보통 swapchain.format()이다.
     * @param[in] depthFormat    깊이 부착물의 형식. FrameLoop::kDepthFormat이다.
     * @param[in] vertexShader   버텍스 단계 SPIR-V 워드 배열.
     * @param[in] fragmentShader 프래그먼트 단계 SPIR-V 워드 배열.
     */
    Renderer(const rhi::Device& device, rhi::Format colorFormat, rhi::Format depthFormat,
             std::span<const std::uint32_t> vertexShader, std::span<const std::uint32_t> fragmentShader);

    /**
     * @brief 패스와 정점 버퍼를 해제한다.
     *
     * GPU가 아직 쓰고 있을 수 있으므로 device.waitIdle() 뒤에 부른다 (frame.md §9).
     */
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /**
     * @brief 이번 프레임의 visible 목록을 기록한다.
     *
     * 프레임 임시 상태에서 render pass 기술자를 만들고, context.drawList의
     * 오브젝트를 삼각형 패스에 넘긴다. 목록이 비어 있어도 clear는 한다 —
     * swapchain 이미지의 내용은 acquire할 때마다 보장되지 않는다 (frame.md §4).
     *
     * @param[in,out] encoder 기록 중인 command buffer의 창구.
     * @param[in]     context 이번 프레임의 부착물 · 크기 · visible 목록.
     * @param[in]     world   전체 오브젝트 수를 로그로 남기기 위한 렌더 월드.
     *
     * @return 이번 프레임에 기록한 draw call 수. context.drawList.size()와 같다.
     */
    std::uint32_t record(rhi::CommandEncoder& encoder, const FrameContext& context, const World& world) const;

private:
    /// @brief 메시를 그리는 패스이다. 셰이더 · 파이프라인 · 정점 버퍼를 소유한다.
    TrianglePass pass_;

    /// @brief 마지막으로 기록한 오브젝트 수이다. 같은 수를 매 프레임 로그하지 않으려고 둔다.
    mutable std::size_t recordedObjectCount_ = 0;

    /// @brief 마지막으로 기록한 visible 수이다. 같은 수를 매 프레임 로그하지 않으려고 둔다.
    mutable std::size_t recordedVisibleCount_ = 0;
};

} // namespace nxt::renderer
