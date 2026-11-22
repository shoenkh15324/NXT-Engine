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
    // 오브젝트 수나 visible 수가 바뀔 때만 남긴다. 매 프레임 로그를 찍으면 측정을 방해한다.
    const std::size_t objectCount = world.objectCount();
    const std::size_t visibleCount = context.drawList.size();
    if ((objectCount != recordedObjectCount_) || (visibleCount != recordedVisibleCount_)) {
        NXT_LOG_INFO(Renderer, Passes, "record: objects={} visible={}", objectCount, visibleCount);
        recordedObjectCount_ = objectCount;
        recordedVisibleCount_ = visibleCount;
    }

    pass_.record(encoder, makeRenderPassDesc(context, kClearColor), context.drawList);

    return static_cast<std::uint32_t>(visibleCount);
}

} // namespace nxt::renderer
