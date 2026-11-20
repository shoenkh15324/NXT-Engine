#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/renderer/mesh/mesh.hpp>
#include <nxt/renderer/world/world.hpp>
#include <span>

namespace nxt::renderer {

namespace rhi = ::nxt::graphics::rhi;

/**
 * @brief Phase 1 1-A1의 렌더 패스이다. 단위 삼각형을 오브젝트마다 그린다.
 *
 * 모든 오브젝트가 같은 단위 삼각형 정점 세 개를 공유한다. 오브젝트마다 다른 것은
 * 변환 행렬과 색뿐이므로, draw 직전에 push constant로 넘긴다. 정점 10만 개를
 * 업로드하지 않으므로 측정 대상이 정점 메모리 대역폭으로 바뀌지 않는다.
 *
 * 인덱스 · 머티리얼 · 텍스처는 아직 없다. 1-A3의 컬링 결과(DrawList)는 이 패스에
 * span으로 전달된다.
 *
 * @note 수명을 가진 객체다. 디바이스보다 먼저 만들어지고 디바이스보다 먼저
 *       파괴되어야 한다.
 * @note Thread safety: thread-safe하지 않다. 렌더 스레드 하나에서만 쓴다.
 */
class TrianglePass {
public:
    /**
     * @brief 파이프라인과 단위 삼각형 정점 버퍼를 만들고 그릴 준비를 한다.
     *
     * 셰이더 모듈은 파이프라인 생성에만 쓰이므로 여기서 만들고 바로 해제한다.
     *
     * @param[in] device         파이프라인을 만들 디바이스. 이 객체보다 오래 산다.
     * @param[in] colorFormat    색 부착물의 형식. RenderPassDesc::colorFormat과 같아야 한다.
     * @param[in] depthFormat    깊이 부착물의 형식. RenderPassDesc::depthFormat과 같아야 한다.
     * @param[in] vertexShader   버텍스 단계 SPIR-V 워드 배열.
     * @param[in] fragmentShader 프래그먼트 단계 SPIR-V 워드 배열.
     */
    TrianglePass(const rhi::Device& device, rhi::Format colorFormat, rhi::Format depthFormat,
                 std::span<const std::uint32_t> vertexShader, std::span<const std::uint32_t> fragmentShader);

    /**
     * @brief 파이프라인과 정점 버퍼를 해제한다.
     *
     * GPU가 아직 쓰고 있을 수 있으므로 device.waitIdle() 뒤에 부른다 (frame.md §9).
     */
    ~TrianglePass();

    TrianglePass(const TrianglePass&) = delete;
    TrianglePass& operator=(const TrianglePass&) = delete;
    TrianglePass(TrianglePass&&) = delete;
    TrianglePass& operator=(TrianglePass&&) = delete;

    /**
     * @brief 오브젝트마다 단위 삼각형 하나를 기록한다.
     *
     * render pass를 열고 clear한 뒤, 오브젝트 순서대로 push constant를 갱신하고
     * draw 하나를 기록한다. 오브젝트가 없어도 pass는 열고 닫는다 — swapchain
     * 이미지의 내용은 acquire할 때마다 보장되지 않으므로 clear가 매 프레임 필요하다.
     *
     * @param[in,out] encoder 기록 중인 command buffer의 창구.
     * @param[in]     target  부착물 · 크기 · clear 값.
     * @param[in]     objects draw 순서대로 정렬된 오브젝트 목록.
     *
     * @note draw call 수는 objects.size()와 같다.
     */
    void record(rhi::CommandEncoder& encoder, const rhi::RenderPassDesc& target,
                std::span<const WorldObject> objects) const;

private:
    /// @brief 단위 삼각형 정점 세 개를 담은 정점 버퍼를 만든다. 생성자에서 한 번만 부른다.
    void createUnitTriangle();

    /// @brief 파이프라인과 버퍼를 만들 디바이스. 이 객체보다 오래 산다.
    const rhi::Device& device_;

    /// @brief 정점을 그리는 파이프라인이다. 생성자에서 만든다.
    rhi::GraphicsPipeline pipeline_{};

    /// @brief 모든 오브젝트가 공유하는 단위 삼각형 정점 버퍼이다.
    rhi::BufferAllocation vertexBuffer_{};
};

} // namespace nxt::renderer
