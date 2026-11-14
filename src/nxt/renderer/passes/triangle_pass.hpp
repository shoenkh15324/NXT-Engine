#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/renderer/mesh/mesh.hpp>
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
 * @brief Phase 0의 렌더 패스이다. 메시의 정점을 삼각형으로 그린다.
 *
 * 파이프라인과 정점 버퍼, 업로드를 이 클래스가 소유한다. Renderer는 메시 목록만
 * 들고 이 패스에 넘긴다 — 셰이더 · 정점 배치 · 파이프라인 상태 같은 삼각형에 대한
 * 지식이 renderer로 새지 않게 하기 위해서다.
 *
 * Phase 0의 셰이더는 변환 행렬을 쓰지 않으므로 MeshVertex::position은 clip 공간
 * 좌표다. 인덱스 · 머티리얼 · 텍스처도 없다. 이 패스는 Phase 2에서 glTF 메시와
 * 머티리얼을 다루는 패스로 대체된다.
 *
 * @note 수명을 가진 객체다. 디바이스보다 먼저 만들어지고 디바이스보다 먼저
 *       파괴되어야 한다.
 * @note Thread safety: thread-safe하지 않다. 렌더 스레드 하나에서만 쓴다.
 */
class TrianglePass {
public:
    /**
     * @brief 파이프라인을 만들고 그릴 준비를 한다.
     *
     * 셰이더 모듈은 파이프라인 생성에만 쓰이므로 여기서 만들고 바로 해제한다.
     * 정점 버퍼는 upload()가 만든다 — 이 시점에 메시가 아직 없을 수 있기 때문이다.
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
     * @brief 메시들의 정점을 하나의 정점 버퍼에 올린다.
     *
     * 메시를 순서대로 이어 붙이고, 메시마다 그 구간을 기억해 두었다가 record()에서
     * draw 하나씩으로 그린다. 정점 수가 0이면 버퍼를 만들지 않는다.
     *
     * @note 다시 부르면 이전 버퍼를 파괴한다. GPU가 아직 그 버퍼를 읽고 있을 수
     *       있으므로 첫 프레임 전이나 device.waitIdle() 뒤에만 부른다 (frame.md §9).
     *
     * @param[in] meshes 올릴 메시들. 버퍼의 수명은 이 목록과 무관하다 — 복사한다.
     */
    void upload(std::span<const Mesh> meshes);

    /**
     * @brief 한 프레임의 이 패스를 기록한다.
     *
     * render pass를 열고 clear한 뒤 정점을 그리고 닫는다. 그릴 메시가 없어도 pass는
     * 열고 닫는다 — swapchain 이미지의 내용은 acquire할 때마다 보장되지 않으므로
     * clear가 매 프레임 필요하다 (frame.md §4).
     *
     * @param[in,out] encoder 기록 중인 command buffer의 창구.
     * @param[in]     target  부착물 · 크기 · clear 값.
     */
    void record(rhi::CommandEncoder& encoder, const rhi::RenderPassDesc& target) const;

private:
    /// @brief 정점 버퍼 안에서 메시 하나가 차지하는 구간이다.
    struct DrawRange {
        /// @brief 이 메시의 첫 정점 인덱스이다. draw()의 firstVertex로 넘긴다.
        std::uint32_t firstVertex = 0;

        /// @brief 이 메시의 정점 수이다. draw()의 vertexCount로 넘긴다.
        std::uint32_t vertexCount = 0;
    };

    /// @brief 파이프라인과 버퍼를 만들 디바이스. 이 객체보다 오래 산다.
    const rhi::Device& device_;

    /// @brief 정점을 그리는 파이프라인이다. 생성자에서 만든다.
    rhi::GraphicsPipeline pipeline_{};

    /// @brief 모든 메시의 정점을 이어 붙인 버퍼이다. upload() 전에는 비어 있다.
    rhi::BufferAllocation vertexBuffer_{};

    /// @brief 메시마다의 draw 구간이다. 메시 순서와 같다.
    std::vector<DrawRange> drawRanges_;
};

} // namespace nxt::renderer
