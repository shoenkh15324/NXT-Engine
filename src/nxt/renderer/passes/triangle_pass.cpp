#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/math/mat4.hpp>
#include <nxt/renderer/passes/triangle_pass.hpp>
#include <span>

namespace nxt::renderer {

namespace {

/// @brief 정점 버퍼를 묶는 번호이다. 지금은 버퍼 하나만 쓴다.
constexpr std::uint32_t kVertexBinding = 0;

/// @brief 단위 삼각형의 정점 수이다. draw마다 이 수만큼 그린다.
constexpr std::uint32_t kUnitTriangleVertexCount = 3;

/**
 * @brief MeshVertex를 셰이더의 location에 잇는 배치이다.
 *
 * triangle.vert는 이제 위치만 정점 입력으로 받는다. 색은 push constant로
 * 넘어가므로 location 1은 쓰지 않는다. stride는 MeshVertex 크기 그대로다.
 */
constexpr std::array<rhi::VertexAttribute, 1> kVertexAttributes{{
    {.location = 0,
     .offset = static_cast<std::uint32_t>(offsetof(MeshVertex, position)),
     .format = rhi::Format::R32G32B32Sfloat},
}};

/// @brief 모든 오브젝트가 공유하는 단위 삼각형 정점이다.
constexpr std::array<MeshVertex, 3> kUnitTriangleVertices{{
    {.position = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 1.0f, 1.0f}},
    {.position = {0.5f, -0.5f, 0.0f}, .color = {1.0f, 1.0f, 1.0f}},
    {.position = {0.0f, 0.5f, 0.0f}, .color = {1.0f, 1.0f, 1.0f}},
}};

/**
 * @brief triangle.vert의 push constant 블록과 같은 배치이다.
 *
 * GLSL std430에서 mat4는 16바이트 정렬로 64바이트를 차지하고, vec4는 그 뒤
 * 64바이트에 놓인다. C++ struct도 같은 순서와 크기여야 한다.
 */
struct TrianglePushConstants {
    math::Mat4 transform;
    float color[4];
};

static_assert(sizeof(math::Mat4) == 64, "Mat4는 64바이트여야 한다.");
static_assert(sizeof(TrianglePushConstants) == 80, "push constant는 80바이트여야 한다.");

constexpr std::uint32_t kPushConstantSize = static_cast<std::uint32_t>(sizeof(TrianglePushConstants));

} // namespace

TrianglePass::TrianglePass(const rhi::Device& device, rhi::Format colorFormat, rhi::Format depthFormat,
                           std::span<const std::uint32_t> vertexShader, std::span<const std::uint32_t> fragmentShader)
    : device_{device} {
    const rhi::Shader vertex = rhi::createShader(device_, vertexShader);
    const rhi::Shader fragment = rhi::createShader(device_, fragmentShader);

    const rhi::VertexLayout vertexLayout{
        .binding = kVertexBinding,
        .stride = static_cast<std::uint32_t>(sizeof(MeshVertex)),
        .attributes = kVertexAttributes,
    };

    const rhi::GraphicsPipelineDesc desc{
        .vertexShader = vertex,
        .fragmentShader = fragment,
        .vertexLayout = vertexLayout,
        // Phase 1 1-A1의 단위 삼각형은 clip 공간 2D라 앞뒤가 없다. 정점을 감는
        // 순서에 따라 사라지지 않게 컬링을 끈다. 3D 메시가 들어오면 다시 정한다.
        .rasterizer = {.cullMode = rhi::CullMode::None, .frontFace = rhi::FrontFace::CounterClockwise},
        .colorFormat = colorFormat,
        .depthFormat = depthFormat,
        .depth = {.testEnabled = true, .writeEnabled = true, .compareOp = rhi::CompareOp::Less},
        .pushConstants = {.stage = rhi::ShaderStage::Vertex, .offset = 0, .size = kPushConstantSize},
    };

    pipeline_ = rhi::createGraphicsPipeline(device_, desc);

    // 파이프라인이 모듈을 다 읽었으므로 바로 해제한다.
    rhi::destroyShader(device_, vertex);
    rhi::destroyShader(device_, fragment);

    createUnitTriangle();

    NXT_LOG_DEBUG(Renderer, Passes, "triangle pass created: pipeline={:#x}, pushConstants={}", pipeline_.value(),
                  kPushConstantSize);
}

TrianglePass::~TrianglePass() {
    // GPU가 아직 이 버퍼를 읽고 있을 수 있다. 호출자가 device.waitIdle()을 마친 뒤에
    // 부른다는 계약이다 (frame.md §9).
    if (vertexBuffer_.buffer.valid()) {
        rhi::destroyBuffer(device_, vertexBuffer_);
    }
    if (pipeline_.valid()) {
        rhi::destroyGraphicsPipeline(device_, pipeline_);
    }
}

void TrianglePass::createUnitTriangle() {
    constexpr std::uint64_t kByteSize = sizeof(kUnitTriangleVertices);

    vertexBuffer_ = rhi::createBuffer(device_, rhi::BufferDesc{
                                                   .size = kByteSize,
                                                   .usage = rhi::BufferUsage::Vertex,
                                               });

    auto* destination = static_cast<MeshVertex*>(rhi::mapMemory(device_, vertexBuffer_.memory, 0, kByteSize));
    std::memcpy(destination, kUnitTriangleVertices.data(), kByteSize);
    rhi::unmapMemory(device_, vertexBuffer_.memory);

    NXT_LOG_DEBUG(Renderer, Passes, "unit triangle uploaded: vertices={}", kUnitTriangleVertexCount);
}

void TrianglePass::record(rhi::CommandEncoder& encoder, const rhi::RenderPassDesc& target,
                          std::span<const WorldObject> objects) const {
    encoder.beginRenderPass(target);

    // 그릴 것이 없어도 pass는 열고 닫는다. clear는 매 프레임 필요하다.
    if (!objects.empty()) {
        encoder.setPipeline(pipeline_);

        // viewport와 scissor는 동적 상태다. 파이프라인에 크기를 박지 않았으므로
        // 그리기 전에 매 프레임 넣어야 한다 (frame.md §6).
        encoder.setViewport(target.extent);
        encoder.setScissor(target.extent);

        encoder.setVertexBuffer(kVertexBinding, vertexBuffer_.buffer);

        for (const WorldObject& object : objects) {
            const TrianglePushConstants constants{
                .transform = object.transform,
                .color = {object.color.x, object.color.y, object.color.z, 1.0f},
            };

            encoder.pushConstants(rhi::ShaderStage::Vertex, std::as_bytes(std::span{&constants, 1}));
            encoder.draw(kUnitTriangleVertexCount, 1, 0, 0);
        }
    }

    encoder.endRenderPass();
}

} // namespace nxt::renderer
