#include <memory>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/linux/wayland/wayland_window.hpp>
#include <nxt/platform/window/window.hpp>

namespace nxt::platform::window {

std::unique_ptr<Window> createWindow(const WindowDesc& desc) {
    // 현재 Linux GUI 백엔드는 Wayland 하나다. --window-system 정책은 L3에서 붙인다.
    auto window = wayland::WaylandWindow::create(desc);
    if (!window) {
        NXT_LOG_ERROR(Platform, Window, "wayland window creation failed");
        return nullptr;
    }
    return window;
}

} // namespace nxt::platform::window
