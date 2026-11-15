#include <nxt/renderer/renderer.hpp>

namespace nxt::renderer {

namespace {

/**
 * @brief 화면을 지우는 색이다.
 *
 * 0-A에서 쓰던 값 그대로다. 배경색은 렌더러의 정책이므로 FrameContext가 아니라
 * 여기서 정한다.
 */
constexpr rhi::ClearColor kClearColor{0.10f, 0.20f, 0.30f, 1.0f};

} // namespace

Renderer::Renderer(const rhi::Device& device, rhi::Format colorFormat, rhi::Format depthFormat,
                   std::span<const std::uint32_t> vertexShader, std::span<const std::uint32_t> fragmentShader)
    : pass_(device, colorFormat, depthFormat, vertexShader, fragmentShader) {}

Renderer::~Renderer() = default;

void Renderer::setMeshes(std::span<const Mesh> meshes) {
    meshes_.assign(meshes.begin(), meshes.end());
    pass_.upload(meshes_);
}

void Renderer::record(rhi::CommandEncoder& encoder, const FrameContext& context) const {
    pass_.record(encoder, makeRenderPassDesc(context, kClearColor));
}

} // namespace nxt::renderer
