#include <nxt/core/diagnostics/log.hpp>
#include <nxt/renderer/renderer.hpp>

namespace nxt::renderer {

namespace {

/**
 * @brief 화면을 지우는 색이다.
 *
 * 배경색은 렌더러의 정책이므로 FrameContext가 아니라 여기서 정한다.
 */
constexpr rhi::ClearColor kClearColor{0.10f, 0.20f, 0.30f, 1.0f};

} // namespace

Renderer::Renderer(const rhi::Device& device, rhi::Format colorFormat, rhi::Format depthFormat,
                   std::span<const std::uint32_t> vertexShader, std::span<const std::uint32_t> fragmentShader)
    : pass_(device, colorFormat, depthFormat, vertexShader, fragmentShader) {}

Renderer::~Renderer() = default;

std::uint32_t Renderer::record(rhi::CommandEncoder& encoder, const FrameContext& context, const World& world) const {
    // 오브젝트 수가 바뀔 때만 남긴다. 매 프레임 로그를 찍으면 측정을 방해한다.
    const std::size_t objectCount = world.objectCount();
    if (objectCount != recordedObjectCount_) {
        NXT_LOG_INFO(Renderer, Passes, "record: objects={} draws={}", objectCount, objectCount);
        recordedObjectCount_ = objectCount;
    }

    pass_.record(encoder, makeRenderPassDesc(context, kClearColor), world.objects());

    // 1-A2는 오브젝트당 draw 하나이므로 draw 수가 곧 오브젝트 수다. 1-A3의 DrawList가
    // 들어오면 그 목록의 크기를 돌려주게 된다.
    return static_cast<std::uint32_t>(objectCount);
}

} // namespace nxt::renderer
