#include <memory>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/linux/wayland/wayland_window.hpp>
#include <nxt/platform/window/window.hpp>

namespace nxt::platform::window {

std::unique_ptr<Window> createWindow(const WindowDesc& desc) {
    // Linux GUI 백엔드는 Wayland 하나다. 창 시스템은 빌드 시 정해지고 실행 중에 고르지 않는다.
    auto window = wayland::WaylandWindow::create(desc);
    if (!window) {
        NXT_LOG_ERROR(Platform, Window, "wayland window creation failed");
        return nullptr;
    }
    return window;
}

} // namespace nxt::platform::window
