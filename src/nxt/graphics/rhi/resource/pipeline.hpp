#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/resource/shader.hpp>
#include <span>

namespace nxt::graphics::rhi {

class Device;

/// @brief GraphicsPipeline을 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct GraphicsPipelineTag {};

/**
 * @brief 정점 버퍼의 속성 하나가 어디에 어떤 형식으로 놓였는지 나타낸다.
 *
 * location은 shader의 layout(location = N)과 맞춘다. offset은 정점 하나의
 * 시작에서 이 속성까지의 바이트 수다.
 */
struct VertexAttribute {
    /// @brief shader가 이 속성을 찾는 번호이다.
    std::uint32_t location = 0;

    /// @brief 정점 시작에서 이 속성까지의 바이트 수이다.
    std::uint32_t offset = 0;

    /// @brief 속성 하나를 이루는 형식이다. 정점 위치라면 R32G32B32Sfloat이다.
    Format format = Format::R32G32B32Sfloat;
};

/**
 * @brief 정점 버퍼 하나의 배치이다.
 *
 * Phase 0은 binding 하나만 쓴다. stride는 정점 하나의 크기이고, attributes의
 * offset은 그 안에서의 위치다.
 *
 * @note attributes는 createGraphicsPipeline()이 반환할 때까지만 유효하면 된다.
 *       Vulkan은 생성 호출에서 값을 복사한다.
 */
struct VertexLayout {
    /// @brief 정점 버퍼를 묶는 번호이다. shader에는 드러나지 않는다.
    std::uint32_t binding = 0;

    /// @brief 정점 하나의 크기이다. 바이트 단위이며 0이면 안 된다.
    std::uint32_t stride = 0;

    /// @brief 이 버퍼가 담은 속성들이다. 비어 있으면 안 된다.
    std::span<const VertexAttribute> attributes;
};

/// @brief 삼각형의 앞면을 어느 쪽으로 볼지 정한다.
struct RasterizerState {
    /// @brief 버릴 면이다. 기본값은 뒷면을 버린다.
    CullMode cullMode = CullMode::Back;

    /// @brief 정점이 감기는 방향 중 어느 쪽을 앞면으로 볼지 정한다.
    FrontFace frontFace = FrontFace::CounterClockwise;
};

/**
 * @brief 깊이 테스트와 깊이 쓰기를 어떻게 할지 정한다.
 *
 * 깊이 테스트가 켜져 있으면 새 조각의 깊이를 깊이 버퍼의 값과 compareOp로 비교하고,
 * 통과한 조각만 색 부착물에 쓴다. writeEnabled가 켜져 있으면 통과한 조각의 깊이로
 * 버퍼를 갱신한다.
 *
 * @note 기본값은 둘 다 꺼짐이다. 깊이 부착물이 있다는 사실만으로 테스트를 켜지
 *       않는다 — 깊이 없이 그리는 패스도 같은 render pass를 쓸 수 있다.
 */
struct DepthState {
    /// @brief 깊이 테스트를 할지 여부이다.
    bool testEnabled = false;

    /// @brief 통과한 조각의 깊이를 버퍼에 쓸지 여부이다.
    bool writeEnabled = false;

    /// @brief 깊이를 비교하는 방법이다. testEnabled가 false면 쓰이지 않는다.
    CompareOp compareOp = CompareOp::Less;
};

/**
 * @brief graphics pipeline을 만드는 데 필요한 설정이다.
 *
 * Phase 0은 버텍스·프래그먼트 두 단계만 쓰고, descriptor set과 push constant가
 * 없으므로 pipeline layout은 비어 있다.
 *
 * 깊이 부착물은 항상 있다. Phase 0의 render pass가 색 부착물과 깊이 부착물을 함께
 * 쓰기 때문이다 (phase0.md B4). 그래서 depthFormat을 비워 둘 수 없고, 깊이 테스트를
 * 켤지는 DepthState가 정한다.
 */
struct GraphicsPipelineDesc {
    /// @brief 버텍스 단계 셰이더이다. 유효해야 한다.
    Shader vertexShader;

    /// @brief 프래그먼트 단계 셰이더이다. 유효해야 한다.
    Shader fragmentShader;

    /// @brief 정점 버퍼의 배치이다.
    VertexLayout vertexLayout;

    /// @brief 정점을 잇는 방식이다.
    Topology topology = Topology::TriangleList;

    /// @brief 면을 버리는 방식이다.
    RasterizerState rasterizer{};

    /// @brief 색 부착물의 형식이다. Swapchain::format()과 같아야 한다.
    Format colorFormat = Format::Undefined;

    /// @brief 깊이 부착물의 형식이다. RenderPassDesc::depthFormat과 같아야 한다.
    Format depthFormat = Format::Undefined;

    /// @brief 깊이 테스트와 깊이 쓰기 상태이다.
    DepthState depth{};
};

/**
 * @brief 정점을 화면에 그리는 데 필요한 상태를 묶은 객체이다.
 *
 * viewport와 scissor는 동적 상태다. 파이프라인에 크기를 박지 않으므로 창 크기가
 * 바뀌어도 다시 만들 필요가 없다. 대신 그리기 전에 encoder가 매 프레임
 * setViewport() · setScissor()로 값을 넣어야 한다 (frame.md §6).
 *
 * @note 값 핸들이다. vk 핸들 외에 자체 상태가 없다 (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권이 없다. createGraphicsPipeline()이 만들고
 *       destroyGraphicsPipeline()이 해제한다.
 */
using GraphicsPipeline = ResourceHandle<GraphicsPipelineTag>;

/**
 * @brief graphics pipeline을 만든다.
 *
 * 파이프라인은 render pass와 호환되어야 하는데, 그 render pass는 그릴 때 encoder가
 * 만드는 것과 구조가 같다. 그래서 이 함수는 잠시 호환용 render pass를 만들어
 * 생성 호출에만 쓰고 바로 파괴한다. 파이프라인은 render pass 핸들을 보관하지
 * 않으므로 이후 그리기에 영향이 없다.
 *
 * @param[in] device 파이프라인을 만들 디바이스.
 * @param[in] desc   파이프라인 설정. 셰이더 두 개와 vertexLayout이 유효해야 한다.
 *
 * @return 만들어진 파이프라인.
 */
[[nodiscard]]
GraphicsPipeline createGraphicsPipeline(const Device& device, const GraphicsPipelineDesc& desc);

/**
 * @brief graphics pipeline을 해제한다.
 *
 * recording 중인 command buffer가 이 파이프라인을 참조하고 있으면 안 된다.
 * 프레임 경계에서 호출한다 (rhi.md §9).
 *
 * @param[in] device   파이프라인을 만든 디바이스.
 * @param[in] pipeline 해제할 파이프라인.
 */
void destroyGraphicsPipeline(const Device& device, const GraphicsPipeline& pipeline);

} // namespace nxt::graphics::rhi
