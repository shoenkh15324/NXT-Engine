#include <nxt/renderer/frame_context.hpp>

namespace nxt::renderer {

rhi::RenderPassDesc makeRenderPassDesc(const FrameContext& context, rhi::ClearColor clearColor) noexcept {
    return rhi::RenderPassDesc{
        .color = context.color,
        .colorFormat = context.colorFormat,
        .extent = context.extent,
        .clearColor = clearColor,
        .depth = context.depth,
        .depthFormat = context.depthFormat,
    };
}

} // namespace nxt::renderer
