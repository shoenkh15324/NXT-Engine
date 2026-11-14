#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/renderer/passes/triangle_pass.hpp>

namespace nxt::renderer {

namespace {

/// @brief 정점 버퍼를 묶는 번호이다. Phase 0은 버퍼 하나만 쓴다.
constexpr std::uint32_t kVertexBinding = 0;

/**
 * @brief MeshVertex를 셰이더의 location에 잇는 배치이다.
 *
 * triangle.vert가 location 0을 위치, 1을 색으로 받는다. offset은 Vec3 두 개가
 * 패딩 없이 이어지므로 MeshVertex 안에서의 실제 위치와 같다.
 */
constexpr std::array<rhi::VertexAttribute, 2> kVertexAttributes{{
    {.location = 0,
     .offset = static_cast<std::uint32_t>(offsetof(MeshVertex, position)),
     .format = rhi::Format::R32G32B32Sfloat},
    {.location = 1,
     .offset = static_cast<std::uint32_t>(offsetof(MeshVertex, color)),
     .format = rhi::Format::R32G32B32Sfloat},
}};

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
        // Phase 0의 정점은 clip 공간의 2D 삼각형이라 앞뒤가 없다. 컬링을 켜면
        // 정점을 감는 순서에 따라 통째로 사라지므로 끈다. 3D 메시가 들어오는
        // Phase 2에서 감는 순서와 함께 다시 정한다.
        .rasterizer = {.cullMode = rhi::CullMode::None, .frontFace = rhi::FrontFace::CounterClockwise},
        .colorFormat = colorFormat,
        .depthFormat = depthFormat,
        .depth = {.testEnabled = true, .writeEnabled = true, .compareOp = rhi::CompareOp::Less},
    };

    pipeline_ = rhi::createGraphicsPipeline(device_, desc);

    // 파이프라인이 모듈을 다 읽었으므로 바로 해제한다. 셰이더 모듈은 GPU 실행 중에
    // 참조되는 리소스가 아니다 (shader.hpp).
    rhi::destroyShader(device_, vertex);
    rhi::destroyShader(device_, fragment);

    NXT_LOG_DEBUG(Renderer, Passes, "triangle pass created: pipeline={:#x}", pipeline_.value());
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

void TrianglePass::upload(std::span<const Mesh> meshes) {
    // 이전 업로드를 먼저 버린다. 매핑은 upload()가 끝날 때 이미 해제했으므로
    // destroyBuffer()를 바로 부를 수 있다.
    if (vertexBuffer_.buffer.valid()) {
        rhi::destroyBuffer(device_, vertexBuffer_);
        vertexBuffer_ = rhi::BufferAllocation{};
    }
    drawRanges_.clear();

    std::size_t vertexCount = 0;
    for (const Mesh& mesh : meshes) {
        vertexCount += mesh.vertices.size();
    }
    if (vertexCount == 0) {
        return;
    }

    const std::size_t byteSize = vertexCount * sizeof(MeshVertex);
    const rhi::BufferDesc desc{
        .size = static_cast<std::uint64_t>(byteSize),
        .usage = rhi::BufferUsage::Vertex,
    };

    vertexBuffer_ = rhi::createBuffer(device_, desc);

    auto* destination = static_cast<MeshVertex*>(rhi::mapMemory(device_, vertexBuffer_.memory, 0, byteSize));

    std::size_t firstVertex = 0;
    for (const Mesh& mesh : meshes) {
        if (mesh.vertices.empty()) {
            continue;
        }

        std::memcpy(destination + firstVertex, mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshVertex));
        drawRanges_.push_back(DrawRange{
            .firstVertex = static_cast<std::uint32_t>(firstVertex),
            .vertexCount = static_cast<std::uint32_t>(mesh.vertices.size()),
        });
        firstVertex += mesh.vertices.size();
    }

    rhi::unmapMemory(device_, vertexBuffer_.memory);

    NXT_LOG_DEBUG(Renderer, Passes, "triangle pass uploaded: meshes={}, vertices={}", meshes.size(), vertexCount);
}

void TrianglePass::record(rhi::CommandEncoder& encoder, const rhi::RenderPassDesc& target) const {
    encoder.beginRenderPass(target);

    // 그릴 것이 없어도 pass는 열고 닫는다. clear는 매 프레임 필요하다.
    if (!drawRanges_.empty()) {
        encoder.setPipeline(pipeline_);

        // viewport와 scissor는 동적 상태다. 파이프라인에 크기를 박지 않았으므로
        // 그리기 전에 매 프레임 넣어야 한다 (frame.md §6).
        encoder.setViewport(target.extent);
        encoder.setScissor(target.extent);

        encoder.setVertexBuffer(kVertexBinding, vertexBuffer_.buffer);

        for (const DrawRange& range : drawRanges_) {
            encoder.draw(range.vertexCount, 1, range.firstVertex, 0);
        }
    }

    encoder.endRenderPass();
}

} // namespace nxt::renderer
