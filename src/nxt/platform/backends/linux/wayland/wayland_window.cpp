#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <libdecor.h>
#include <memory>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/linux/wayland/wayland_window.hpp>
#include <nxt/platform/window/window.hpp>
#include <poll.h>
#include <string>
#include <utility>
#include <wayland-client.h>

// wayland-scanner가 빌드 디렉터리에 생성한다 (platform/CMakeLists.txt).
// 소스가 아니라 생성물이므로 저장소에 커밋하지 않는다. libdecor를 쓸 수 없는
// 환경에서만 xdg-shell을 직접 쓴다.
#include "xdg-shell-client-protocol.h"

namespace nxt::platform::wayland {

using nxt::platform::window::NativeWindow;
using nxt::platform::window::WindowDesc;

namespace {

/// @brief desc 크기가 0일 때 쓰는 논리 크기이다.
constexpr std::uint32_t kFallbackWidth = 1280;
constexpr std::uint32_t kFallbackHeight = 720;

/// @brief 첫 configure를 기다릴 때 허용하는 roundtrip 횟수이다.
constexpr int kInitialConfigureRoundtrips = 3;

/// @brief libdecor 첫 configure를 기다리는 최대 시간이다.
constexpr auto kDecorConfigureTimeout = std::chrono::seconds(2);

/// @brief libdecor_dispatch 한 번에 기다리는 시간이다. 이벤트가 없으면 곧 돌아온다.
constexpr int kDecorDispatchSliceMs = 50;

/// @brief wl_output.scale을 받기 위해 바인딩하는 최대 버전이다.
constexpr std::uint32_t kMaxOutputVersion = 2;

/// @brief compositor 버전 3의 wl_surface.set_buffer_scale을 쓰기 위해 4까지 바인딩한다.
constexpr std::uint32_t kMaxCompositorVersion = 4;

/// @brief wl_surface.set_buffer_scale이 추가된 compositor 버전이다.
constexpr std::uint32_t kBufferScaleSinceVersion = 3;

} // namespace

/**
 * @brief Wayland C 콜백을 WaylandWindow의 private 상태에 연결한다.
 *
 * Wayland는 콜백마다 void* 데이터를 넘기므로, 이 구조체를 friend로 선언해
 * WaylandWindow의 상태를 직접 갱신한다. 리스너 구조체의 함수 포인터는 아래
 * 익명 namespace 상수에서 이 함수들을 가리킨다.
 */
struct WaylandCallbacks {
    static void global(void* data, wl_registry* registry, std::uint32_t name, const char* interface,
                       std::uint32_t version);
    static void globalRemove(void* data, wl_registry* registry, std::uint32_t name);
    static void ping(void* data, xdg_wm_base* wmBase, std::uint32_t serial);
    static void xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial);
    static void toplevelConfigure(void* data, xdg_toplevel* toplevel, std::int32_t width, std::int32_t height,
                                  wl_array* states);
    static void toplevelClose(void* data, xdg_toplevel* toplevel);
    static void surfaceEnter(void* data, wl_surface* surface, wl_output* output);
    static void surfaceLeave(void* data, wl_surface* surface, wl_output* output);
    static void outputScale(void* data, wl_output* output, std::int32_t factor);
    static void outputGeometry(void* data, wl_output* output, std::int32_t x, std::int32_t y,
                               std::int32_t physicalWidth, std::int32_t physicalHeight, std::int32_t subpixel,
                               const char* make, const char* model, std::int32_t transform);
    static void outputMode(void* data, wl_output* output, std::uint32_t flags, std::int32_t width, std::int32_t height,
                           std::int32_t refresh);
    static void outputDone(void* data, wl_output* output);

    // libdecor 경로의 콜백이다. libdecor가 xdg_surface/xdg_toplevel을 소유한다.
    static void decorError(libdecor* context, libdecor_error error, const char* message);
    static void frameConfigure(libdecor_frame* frame, libdecor_configuration* configuration, void* data);
    static void frameClose(libdecor_frame* frame, void* data);
    static void frameCommit(libdecor_frame* frame, void* data);
};

namespace {

const wl_registry_listener kRegistryListener = {
    .global = &WaylandCallbacks::global,
    .global_remove = &WaylandCallbacks::globalRemove,
};

const xdg_wm_base_listener kWmBaseListener = {
    .ping = &WaylandCallbacks::ping,
};

const xdg_surface_listener kXdgSurfaceListener = {
    .configure = &WaylandCallbacks::xdgSurfaceConfigure,
};

// Wayland 헤더 버전에 따라 리스너 구조체 뒤에 필드가 더 붙는다. 일부만 지정
// 초기화하면 -Wmissing-field-initializers가 뜨므로 0으로 초기화한 뒤 채운다.
const xdg_toplevel_listener kToplevelListener = [] {
    xdg_toplevel_listener listener{};
    listener.configure = &WaylandCallbacks::toplevelConfigure;
    listener.close = &WaylandCallbacks::toplevelClose;
    return listener;
}();

const wl_surface_listener kSurfaceListener = [] {
    wl_surface_listener listener{};
    listener.enter = &WaylandCallbacks::surfaceEnter;
    listener.leave = &WaylandCallbacks::surfaceLeave;
    return listener;
}();

const wl_output_listener kOutputListener = [] {
    wl_output_listener listener{};
    listener.geometry = &WaylandCallbacks::outputGeometry;
    listener.mode = &WaylandCallbacks::outputMode;
    listener.done = &WaylandCallbacks::outputDone;
    listener.scale = &WaylandCallbacks::outputScale;
    return listener;
}();

// libdecor 인터페이스도 reserved 필드가 있어 0으로 초기화한 뒤 채운다.
// libdecor API가 const 포인터를 받지 않으므로 상수로 두지 않는다.
libdecor_interface kDecorInterface = [] {
    libdecor_interface iface{};
    iface.error = &WaylandCallbacks::decorError;
    return iface;
}();

libdecor_frame_interface kFrameInterface = [] {
    libdecor_frame_interface iface{};
    iface.configure = &WaylandCallbacks::frameConfigure;
    iface.close = &WaylandCallbacks::frameClose;
    iface.commit = &WaylandCallbacks::frameCommit;
    return iface;
}();

} // namespace

std::unique_ptr<WaylandWindow> WaylandWindow::create(const WindowDesc& desc) {
    // make_unique는 private 생성자를 부를 수 없다. 여기서만 new를 쓴다.
    auto window = std::unique_ptr<WaylandWindow>(new WaylandWindow());
    if (!window->initialize(desc)) {
        return nullptr;
    }
    return window;
}

WaylandWindow::~WaylandWindow() {
    destroyWaylandObjects();
}

bool WaylandWindow::initialize(const WindowDesc& desc) {
    logicalWidth_ = (desc.width > 0) ? desc.width : kFallbackWidth;
    logicalHeight_ = (desc.height > 0) ? desc.height : kFallbackHeight;
    title_ = std::string(desc.title);

    display_ = wl_display_connect(nullptr);
    if (display_ == nullptr) {
        const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
        NXT_LOG_ERROR(Platform, Window, "wl_display_connect failed (WAYLAND_DISPLAY={})",
                      (waylandDisplay != nullptr) ? waylandDisplay : "(unset)");
        return false;
    }

    registry_ = wl_display_get_registry(display_);
    if (registry_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "wl_display_get_registry failed");
        return false;
    }
    wl_registry_add_listener(registry_, &kRegistryListener, this);

    // 첫 roundtrip에서 compositor와 xdg_wm_base를 바인딩한다. compositor가 없으면
    // 표면을 만들 수 없다. xdg_wm_base는 장식 없는 폴백 경로에서만 쓴다.
    if (wl_display_roundtrip(display_) < 0) {
        NXT_LOG_ERROR(Platform, Window, "wayland roundtrip failed while binding globals");
        return false;
    }

    if (compositor_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "required wayland global is missing: compositor");
        return false;
    }

    surface_ = wl_compositor_create_surface(compositor_);
    if (surface_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "wl_compositor_create_surface failed");
        return false;
    }
    wl_surface_add_listener(surface_, &kSurfaceListener, this);

    // 장식이 있는 창이 기본이다. libdecor를 쓸 수 없는 환경에서는 장식 없이 계속한다.
    if (initializeDecorated()) {
        NXT_LOG_INFO(Platform, Window, "wayland window ready: {}x{} (logical {}x{}, scale {}, libdecor)", width(),
                     height(), logicalWidth_, logicalHeight_, scale_);
        return true;
    }

    // libdecor가 표면에 xdg 역할을 붙인 뒤(map) 실패하면 무장식으로 되돌릴 수 없다.
    // 역할을 붙이기 전에 실패한 경우에만 폴백한다.
    if (frame_ != nullptr) {
        return false;
    }

    NXT_LOG_WARN(Platform, Window, "libdecor unavailable; continuing without window decorations");
    if (!initializeUndecorated()) {
        return false;
    }

    NXT_LOG_INFO(Platform, Window, "wayland window ready: {}x{} (logical {}x{}, scale {}, xdg-shell)", width(),
                 height(), logicalWidth_, logicalHeight_, scale_);
    return true;
}

bool WaylandWindow::initializeDecorated() {
    decor_ = libdecor_new(display_, &kDecorInterface);
    if (decor_ == nullptr) {
        return false;
    }

    frame_ = libdecor_decorate(decor_, surface_, &kFrameInterface, this);
    if (frame_ == nullptr) {
        NXT_LOG_WARN(Platform, Window, "libdecor_decorate failed");
        libdecor_unref(decor_);
        decor_ = nullptr;
        return false;
    }

    libdecor_frame_set_title(frame_, title_.c_str());
    libdecor_frame_set_app_id(frame_, "nxt.sandbox");

    // map이 첫 wl_surface commit까지 한다. 이 commit 뒤에 configure가 온다.
    // 여기서부터 표면은 xdg_toplevel 역할을 가지므로 무장식으로 폴백할 수 없다.
    libdecor_frame_map(frame_);

    // configure와 ack는 libdecor가 처리하고 frameConfigure 콜백으로 크기만 받는다.
    const auto deadline = std::chrono::steady_clock::now() + kDecorConfigureTimeout;
    while (!configured_ && !closed_ && (std::chrono::steady_clock::now() < deadline)) {
        if (libdecor_dispatch(decor_, kDecorDispatchSliceMs) < 0) {
            NXT_LOG_ERROR(Platform, Window, "libdecor dispatch failed while waiting for initial configure");
            return false;
        }
        // libdecor는 자기 큐만 디스패치한다. 우리 큐(출력·registry)도 처리한다.
        if (wl_display_dispatch_pending(display_) < 0) {
            NXT_LOG_ERROR(Platform, Window, "wayland dispatch_pending failed while waiting for initial configure");
            return false;
        }
    }

    if (closed_) {
        NXT_LOG_ERROR(Platform, Window, "wayland window closed before initial configure");
        return false;
    }
    if (!configured_) {
        NXT_LOG_ERROR(Platform, Window, "libdecor initial configure was not received");
        return false;
    }

    return true;
}

bool WaylandWindow::initializeUndecorated() {
    if (wmBase_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "xdg_wm_base is missing; cannot create an undecorated window");
        return false;
    }

    xdgSurface_ = xdg_wm_base_get_xdg_surface(wmBase_, surface_);
    if (xdgSurface_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "xdg_wm_base_get_xdg_surface failed");
        return false;
    }
    xdg_surface_add_listener(xdgSurface_, &kXdgSurfaceListener, this);

    toplevel_ = xdg_surface_get_toplevel(xdgSurface_);
    if (toplevel_ == nullptr) {
        NXT_LOG_ERROR(Platform, Window, "xdg_surface_get_toplevel failed");
        return false;
    }
    xdg_toplevel_add_listener(toplevel_, &kToplevelListener, this);
    xdg_toplevel_set_title(toplevel_, title_.c_str());
    xdg_toplevel_set_app_id(toplevel_, "nxt.sandbox");

    // xdg_surface.configure는 첫 wl_surface commit 이후에 온다. configure를 받고
    // ack해야 buffer를 붙일 수 있으므로, 창을 넘기기 전에 여기서 기다린다.
    wl_surface_commit(surface_);
    for (int attempt = 0; (attempt < kInitialConfigureRoundtrips) && !configured_ && !closed_; ++attempt) {
        if (wl_display_roundtrip(display_) < 0) {
            NXT_LOG_ERROR(Platform, Window, "wayland roundtrip failed while waiting for initial configure");
            return false;
        }
    }

    if (closed_) {
        NXT_LOG_ERROR(Platform, Window, "wayland window closed before initial configure");
        return false;
    }
    if (!configured_) {
        NXT_LOG_ERROR(Platform, Window, "wayland initial configure was not received");
        return false;
    }

    return true;
}

NativeWindow WaylandWindow::native() const noexcept {
    // Wayland의 Vulkan surface는 wl_display와 wl_surface를 함께 요구한다.
    // NativeWindow가 그 두 포인터를 그대로 전달한다 (window.hpp).
    return NativeWindow{surface_, display_};
}

std::uint32_t WaylandWindow::width() const noexcept {
    // xdg_toplevel configure는 논리 크기를 준다. swapchain은 buffer 픽셀을 요구하므로
    // 정수 scale을 곱한다. fractional scale은 이 범위에서 지원하지 않는다.
    return logicalWidth_ * static_cast<std::uint32_t>(scale_);
}

std::uint32_t WaylandWindow::height() const noexcept {
    return logicalHeight_ * static_cast<std::uint32_t>(scale_);
}

bool WaylandWindow::isOpen() const noexcept {
    return !closed_;
}

void WaylandWindow::pollEvents() {
    if ((display_ == nullptr) || closed_) {
        return;
    }

    if (decor_ != nullptr) {
        // libdecor가 소켓을 읽고 자기 큐를 디스패치한다. timeout 0이라 프레임을 막지 않는다.
        // 우리가 wl_display_prepare_read를 잡으면 libdecor와 충돌하므로 섞지 않는다.
        if (libdecor_dispatch(decor_, 0) < 0) {
            NXT_LOG_ERROR(Platform, Window, "libdecor dispatch failed");
            closed_ = true;
            return;
        }

        // libdecor는 자기 큐만 디스패치한다. default 큐의 이벤트(출력 scale,
        // surface enter/leave, registry)는 여기서 처리한다.
        if (wl_display_dispatch_pending(display_) < 0) {
            NXT_LOG_ERROR(Platform, Window, "wayland dispatch_pending failed");
            closed_ = true;
        }
        return;
    }

    // 이미 읽은 이벤트를 먼저 처리한다. 여기서 configure/close 콜백이 불린다.
    if (wl_display_dispatch_pending(display_) < 0) {
        NXT_LOG_ERROR(Platform, Window, "wayland dispatch_pending failed");
        closed_ = true;
        return;
    }

    // prepare_read → flush → poll(0) → read_events/cancel_read 순서를 지켜야
    // 이벤트를 기다리며 프레임을 막지 않는다. 다른 스레드가 읽기 준비 중이면
    // (단일 스레드 전제에서는 발생하지 않는다) 이번 프레임은 건너뛴다.
    if (wl_display_prepare_read(display_) != 0) {
        return;
    }

    if ((wl_display_flush(display_) < 0) && (errno != EAGAIN)) {
        wl_display_cancel_read(display_);
        NXT_LOG_ERROR(Platform, Window, "wayland flush failed: errno={}", errno);
        closed_ = true;
        return;
    }

    pollfd descriptor{};
    descriptor.fd = wl_display_get_fd(display_);
    descriptor.events = POLLIN;

    // timeout 0: 이벤트가 없으면 즉시 돌아간다. 메인 프레임을 무기한 막지 않는다.
    const int ready = ::poll(&descriptor, 1, 0);
    if (ready < 0) {
        wl_display_cancel_read(display_);
        if (errno != EINTR) {
            NXT_LOG_ERROR(Platform, Window, "wayland poll failed: errno={}", errno);
            closed_ = true;
        }
        return;
    }

    if ((ready == 0) || ((descriptor.revents & POLLIN) == 0)) {
        wl_display_cancel_read(display_);
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            NXT_LOG_ERROR(Platform, Window, "wayland display disconnected");
            closed_ = true;
        }
        return;
    }

    if (wl_display_read_events(display_) < 0) {
        NXT_LOG_ERROR(Platform, Window, "wayland read_events failed");
        closed_ = true;
        return;
    }
    if (wl_display_dispatch_pending(display_) < 0) {
        NXT_LOG_ERROR(Platform, Window, "wayland dispatch_pending after read failed");
        closed_ = true;
    }
}

void WaylandWindow::destroyWaylandObjects() noexcept {
    // Surface → Wayland 객체 → 전역 → display 연결 순서로 정리한다.
    // graphics 계층의 VkSurfaceKHR는 Engine이 이 창보다 먼저 파괴한다.
    for (const auto& binding : outputs_) {
        if (binding->output != nullptr) {
            wl_output_destroy(binding->output);
        }
    }
    outputs_.clear();
    currentOutput_ = nullptr;

    // libdecor가 xdg_surface/toplevel과 장식 subsurface를 소유한다. wl_surface보다
    // 먼저 정리해야 한다.
    if (frame_ != nullptr) {
        libdecor_frame_unref(frame_);
        frame_ = nullptr;
    }
    if (decor_ != nullptr) {
        libdecor_unref(decor_);
        decor_ = nullptr;
    }

    if (toplevel_ != nullptr) {
        xdg_toplevel_destroy(toplevel_);
        toplevel_ = nullptr;
    }
    if (xdgSurface_ != nullptr) {
        xdg_surface_destroy(xdgSurface_);
        xdgSurface_ = nullptr;
    }
    if (surface_ != nullptr) {
        wl_surface_destroy(surface_);
        surface_ = nullptr;
    }
    if (wmBase_ != nullptr) {
        xdg_wm_base_destroy(wmBase_);
        wmBase_ = nullptr;
    }
    if (compositor_ != nullptr) {
        wl_compositor_destroy(compositor_);
        compositor_ = nullptr;
    }
    if (registry_ != nullptr) {
        wl_registry_destroy(registry_);
        registry_ = nullptr;
    }
    if (display_ != nullptr) {
        wl_display_flush(display_);
        wl_display_disconnect(display_);
        display_ = nullptr;
    }
}

void WaylandWindow::applyOutputScale(wl_output* output) noexcept {
    for (const auto& binding : outputs_) {
        if (binding->output == output) {
            applyScale(binding->scale);
            return;
        }
    }
}

void WaylandWindow::applyScale(std::int32_t scale) noexcept {
    // set_buffer_scale은 compositor 버전 3부터다. 그보다 낮은 compositor에서는
    // 배율을 바꿀 수 없으므로 1로 둔다. width()가 scale_을 곱하므로 여기서 값을
    // 바꾸지 않아야 buffer 크기와 어긋나지 않는다.
    if (compositorVersion_ < kBufferScaleSinceVersion) {
        return;
    }

    const std::int32_t normalized = (scale > 0) ? scale : 1;
    if ((normalized == scale_) || (surface_ == nullptr)) {
        return;
    }
    scale_ = normalized;
    // buffer scale을 바꾸면 다음 buffer의 픽셀 크기가 달라진다. Engine은 width()와
    // swapchain extent를 비교하므로 다음 프레임에서 재생성한다.
    wl_surface_set_buffer_scale(surface_, scale_);
    NXT_LOG_DEBUG(Platform, Window, "wayland buffer scale changed: {}", scale_);
}

// ---- WaylandCallbacks ----

void WaylandCallbacks::global(void* data, wl_registry* registry, std::uint32_t name, const char* interface,
                              std::uint32_t version) {
    auto* self = static_cast<WaylandWindow*>(data);

    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        const std::uint32_t bindVersion = std::min(version, kMaxCompositorVersion);
        self->compositor_ =
            static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
        if (self->compositor_ != nullptr) {
            self->compositorVersion_ = bindVersion;
        }
        return;
    }

    if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        self->wmBase_ = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        if (self->wmBase_ != nullptr) {
            xdg_wm_base_add_listener(self->wmBase_, &kWmBaseListener, self);
        }
        return;
    }

    if (std::strcmp(interface, wl_output_interface.name) == 0) {
        // 출력마다 scale을 따로 받아 둔다. wl_surface.enter에서 어느 출력인지 알려 준다.
        auto binding = std::make_unique<WaylandWindow::OutputBinding>();
        binding->owner = self;
        binding->name = name;
        binding->output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min(version, kMaxOutputVersion)));
        if (binding->output != nullptr) {
            wl_output_add_listener(binding->output, &kOutputListener, binding.get());
        }
        self->outputs_.push_back(std::move(binding));
    }
}

void WaylandCallbacks::globalRemove(void* data, wl_registry*, std::uint32_t name) {
    auto* self = static_cast<WaylandWindow*>(data);

    const auto found = std::find_if(self->outputs_.begin(), self->outputs_.end(),
                                    [name](const auto& binding) { return binding->name == name; });
    if (found == self->outputs_.end()) {
        return;
    }

    if (self->currentOutput_ == (*found)->output) {
        self->currentOutput_ = nullptr;
    }
    if ((*found)->output != nullptr) {
        wl_output_destroy((*found)->output);
    }
    self->outputs_.erase(found);
}

void WaylandCallbacks::ping(void*, xdg_wm_base* wmBase, std::uint32_t serial) {
    // 응답하지 않으면 compositor가 클라이언트를 응답 없음으로 판단한다.
    xdg_wm_base_pong(wmBase, serial);
}

void WaylandCallbacks::xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto* self = static_cast<WaylandWindow*>(data);
    // configure를 ack해야 다음 buffer를 붙일 수 있다. 크기는 이미
    // toplevelConfigure가 logicalWidth_/logicalHeight_에 반영했다.
    xdg_surface_ack_configure(surface, serial);
    self->configured_ = true;
}

void WaylandCallbacks::toplevelConfigure(void* data, xdg_toplevel*, std::int32_t width, std::int32_t height,
                                         wl_array*) {
    auto* self = static_cast<WaylandWindow*>(data);

    // 0은 "클라이언트가 정한다"는 뜻이다. 최소화로 해석하지 않고 이전 크기를 유지한다.
    if (width > 0) {
        self->logicalWidth_ = static_cast<std::uint32_t>(width);
    }
    if (height > 0) {
        self->logicalHeight_ = static_cast<std::uint32_t>(height);
    }

    NXT_LOG_DEBUG(Platform, Window, "wayland configure: {}x{} logical", self->logicalWidth_, self->logicalHeight_);
}

void WaylandCallbacks::toplevelClose(void* data, xdg_toplevel*) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->closed_ = true;
    NXT_LOG_INFO(Platform, Window, "wayland close requested");
}

void WaylandCallbacks::surfaceEnter(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->currentOutput_ = output;
    self->applyOutputScale(output);
}

void WaylandCallbacks::surfaceLeave(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (self->currentOutput_ == output) {
        self->currentOutput_ = nullptr;
    }
}

void WaylandCallbacks::outputScale(void* data, wl_output*, std::int32_t factor) {
    auto* binding = static_cast<WaylandWindow::OutputBinding*>(data);
    binding->scale = (factor > 0) ? factor : 1;

    if ((binding->owner != nullptr) && (binding->owner->currentOutput_ == binding->output)) {
        binding->owner->applyScale(binding->scale);
    }
}

// geometry/mode/done은 지금 범위에서 쓰지 않는다. 리스너 구조체를 채우기 위해 둔다.
void WaylandCallbacks::outputGeometry(void*, wl_output*, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                      std::int32_t, const char*, const char*, std::int32_t) {}

void WaylandCallbacks::outputMode(void*, wl_output*, std::uint32_t, std::int32_t, std::int32_t, std::int32_t) {}

void WaylandCallbacks::outputDone(void*, wl_output*) {}

// ---- libdecor ----

void WaylandCallbacks::decorError(libdecor*, libdecor_error error, const char* message) {
    // 창을 계속 쓸 수 있는 오류도 있으므로 닫지 않고 원인만 남긴다.
    NXT_LOG_ERROR(Platform, Window, "libdecor error {}: {}", static_cast<int>(error),
                  (message != nullptr) ? message : "(no message)");
}

void WaylandCallbacks::frameConfigure(libdecor_frame* frame, libdecor_configuration* configuration, void* data) {
    auto* self = static_cast<WaylandWindow*>(data);

    int width = 0;
    int height = 0;
    // 크기가 없는 configure(최소화 등)는 이전 논리 크기를 유지한다.
    if (!libdecor_configuration_get_content_size(configuration, frame, &width, &height)) {
        width = static_cast<int>(self->logicalWidth_);
        height = static_cast<int>(self->logicalHeight_);
    }
    // 0은 "클라이언트가 정한다"는 뜻이다. 최소화로 해석하지 않고 이전 크기를 유지한다.
    if (width <= 0) {
        width = static_cast<int>(self->logicalWidth_);
    }
    if (height <= 0) {
        height = static_cast<int>(self->logicalHeight_);
    }

    // content 크기는 장식을 뺀 표면 크기다. Vulkan이 그리는 영역과 같으므로 그대로 쓴다.
    // commit이 configure ack를 겸한다. 이걸 빠뜨리면 buffer를 붙일 수 없다.
    libdecor_state* state = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, state, configuration);
    libdecor_state_free(state);

    self->logicalWidth_ = static_cast<std::uint32_t>(width);
    self->logicalHeight_ = static_cast<std::uint32_t>(height);
    self->configured_ = true;

    NXT_LOG_DEBUG(Platform, Window, "libdecor configure: {}x{} content", width, height);
}

void WaylandCallbacks::frameClose(libdecor_frame*, void* data) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->closed_ = true;
    NXT_LOG_INFO(Platform, Window, "wayland close requested");
}

void WaylandCallbacks::frameCommit(libdecor_frame*, void* data) {
    // 동기 subsurface로 장식을 그리는 플러그인은 메인 표면 commit을 요구한다.
    // buffer를 새로 붙이지 않는 commit이라 표면 상태만 갱신된다.
    auto* self = static_cast<WaylandWindow*>(data);
    wl_surface_commit(self->surface_);
}

} // namespace nxt::platform::wayland
