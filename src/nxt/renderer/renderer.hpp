#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/renderer/frame_context.hpp>
#include <nxt/renderer/mesh/mesh.hpp>
#include <nxt/renderer/passes/triangle_pass.hpp>
#include <span>
#include <vector>

namespace nxt::renderer {

/**
 * @brief RHI 타입을 짧게 쓰기 위한 별칭이다.
 *
 * 헤더에서 using namespace를 전개하면 이 헤더를 include하는 모든 TU에 rhi의 이름이
 * 풀린다. 공개 계약인 헤더에서는 별칭 하나만 노출하고 타입은 rhi::로 한정한다.
 */
namespace rhi = ::nxt::graphics::rhi;

/**
 * @brief 한 프레임에 그릴 것을 조립해 encoder에 기록한다.
 *
 * 패스를 나열하고, 이번 프레임의 부착물에 맞춰 각 패스를 기록한다 (frame.md §3).
 * acquire · submit · present · 동기화는 모른다 — 그 순서는 engine의 프레임 루프가
 * 정하고, renderer는 "무엇을"만 정한다.
 *
 * Phase 0은 패스가 하나뿐이라 목록도 그래프도 없다. 메시 N개를 삼각형 패스 하나로
 * 그린다. Renderer는 메시 목록과 패스의 호출 순서만 들고, 셰이더 · 파이프라인 ·
 * 정점 버퍼 같은 삼각형에 대한 지식은 TrianglePass가 소유한다.
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
     * @brief 그릴 메시 목록을 통째로 바꾼다.
     *
     * 목록을 복사해 두었다가 정점 버퍼를 다시 만든다. 메시 개수를 바꾸면 다음
     * 프레임부터 그 개수만큼 그린다.
     *
     * @note 정점 버퍼를 다시 만들므로 GPU가 아직 이전 버퍼를 읽고 있으면 안 된다.
     *       첫 프레임 전이나 device.waitIdle() 뒤에만 부른다 (frame.md §9).
     *
     * @param[in] meshes 새로 그릴 메시들. 복사하므로 호출 뒤에 버려도 된다.
     */
    void setMeshes(std::span<const Mesh> meshes);

    /**
     * @brief 이번 프레임에 그릴 것을 encoder에 기록한다.
     *
     * 프레임 임시 상태에서 render pass 기술자를 만들고 삼각형 패스에 넘긴다.
     * 메시가 없어도 clear는 한다 — swapchain 이미지의 내용은 acquire할 때마다
     * 보장되지 않는다 (frame.md §4).
     *
     * @param[in,out] encoder 기록 중인 command buffer의 창구.
     * @param[in]     context 이번 프레임의 부착물 · 크기.
     */
    void record(rhi::CommandEncoder& encoder, const FrameContext& context) const;

private:
    /// @brief 메시를 그리는 패스이다. 셰이더 · 파이프라인 · 정점 버퍼를 소유한다.
    TrianglePass pass_;

    /// @brief 이번에 그릴 메시의 CPU 사본이다. setMeshes()가 통째로 바꾼다.
    std::vector<Mesh> meshes_;
};

} // namespace nxt::renderer
