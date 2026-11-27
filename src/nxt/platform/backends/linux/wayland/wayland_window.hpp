#pragma once

#include <cstdint>
#include <memory>
#include <nxt/platform/window/window.hpp>
#include <string>
#include <vector>

struct libdecor;
struct libdecor_frame;
struct wl_array;
struct wl_compositor;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;

namespace nxt::platform::wayland {

struct WaylandCallbacks;

/**
 * @brief Wayland 창이다. 기본은 libdecor 장식, 안 되면 xdg-shell 무장식이다.
 *
 * wl_display 연결, registry 바인딩, wl_surface 생성과 configure 수명을 소유한다.
 * 장식은 libdecor가 그린다. libdecor를 쓸 수 없는 환경에서는 xdg_surface/xdg_toplevel을
 * 직접 만들어 장식 없이 계속 동작한다. Vulkan surface는 이 객체의 native()가 넘기는
 * wl_display·wl_surface로 graphics 계층이 만든다.
 *
 * @note thread-safe하지 않다. 생성·pollEvents·파괴는 같은 스레드에서 한다.
 * @note 창이 닫히거나 연결이 끊기면 isOpen()이 false가 된다.
 */
class WaylandWindow final : public window::Window {
public:
    /**
     * @brief Wayland 연결과 창을 만들고 첫 configure까지 기다린다.
     *
     * @param[in] desc 창 제목과 초기 논리 크기.
     * @return 만들어진 창. 실패하면 nullptr이며 원인은 로그로 남는다.
     */
    [[nodiscard]]
    static std::unique_ptr<WaylandWindow> create(const window::WindowDesc& desc);

    ~WaylandWindow() override;

    WaylandWindow(const WaylandWindow&) = delete;
    WaylandWindow& operator=(const WaylandWindow&) = delete;
    WaylandWindow(WaylandWindow&&) = delete;
    WaylandWindow& operator=(WaylandWindow&&) = delete;

    /// @brief wl_surface와 wl_display를 반환한다.
    [[nodiscard]]
    window::NativeWindow native() const noexcept override;

    /// @brief buffer 픽셀 기준 가로 크기(논리 크기 x 정수 scale)를 반환한다.
    [[nodiscard]]
    std::uint32_t width() const noexcept override;

    /// @brief buffer 픽셀 기준 세로 크기를 반환한다.
    [[nodiscard]]
    std::uint32_t height() const noexcept override;

    /// @brief 닫기 요청이나 연결 오류가 없었는지 반환한다.
    [[nodiscard]]
    bool isOpen() const noexcept override;

    /// @brief 대기 중인 Wayland 이벤트를 블로킹 없이 처리한다.
    void pollEvents() override;

private:
    friend struct WaylandCallbacks;

    WaylandWindow() = default;

    [[nodiscard]]
    bool initialize(const window::WindowDesc& desc);

    /// @brief libdecor로 장식 있는 창을 만든다.
    ///
    /// 실패해도 frame_이 nullptr이면 무장식 경로로 폴백할 수 있다. libdecor가 표면에
    /// xdg 역할을 붙인 뒤(map) 실패하면 폴백할 수 없으므로 frame_이 남는다.
    [[nodiscard]]
    bool initializeDecorated();

    /// @brief xdg-shell로 장식 없는 창을 만든다 (libdecor 폴백).
    [[nodiscard]]
    bool initializeUndecorated();

    /// @brief 만든 Wayland 객체를 Surface → 전역 → display 순서로 파괴한다.
    void destroyWaylandObjects() noexcept;
    void applyOutputScale(wl_output* output) noexcept;
    void applyScale(std::int32_t scale) noexcept;

    /// @brief 바인딩한 wl_output 하나와 그 scale을 보관한다.
    struct OutputBinding {
        WaylandWindow* owner{nullptr};
        wl_output* output{nullptr};
        std::uint32_t name{0};
        std::int32_t scale{1};
    };

    wl_display* display_{nullptr};       ///< compositor 연결. native()의 display가 된다.
    wl_registry* registry_{nullptr};     ///< 전역 인터페이스 목록. 생성 중에만 쓴다.
    wl_compositor* compositor_{nullptr}; ///< wl_surface를 만드는 전역.
    std::uint32_t compositorVersion_{0}; ///< 바인딩한 compositor 버전. set_buffer_scale 가능 여부를 가른다.
    xdg_wm_base* wmBase_{nullptr};       ///< xdg_surface를 만드는 전역. ping에 응답해야 한다.
    wl_surface* surface_{nullptr};       ///< Vulkan이 buffer를 붙이는 표면. native()의 handle이 된다.
    libdecor* decor_{nullptr};           ///< 장식 컨텍스트. nullptr이면 무장식 경로다.
    libdecor_frame* frame_{nullptr};     ///< 장식이 붙은 창. xdg_surface/toplevel을 소유한다.
    xdg_surface* xdgSurface_{nullptr};   ///< 무장식 경로의 configure/ack 표면. 장식 경로에서는 nullptr.
    xdg_toplevel* toplevel_{nullptr};    ///< 무장식 경로의 최상위 창. 장식 경로에서는 nullptr.
    std::vector<std::unique_ptr<OutputBinding>> outputs_; ///< scale을 읽기 위해 바인딩한 출력들.
    wl_output* currentOutput_{nullptr};  ///< 이 창이 올라가 있는 출력. 없으면 nullptr.

    std::string title_;                  ///< 창 제목. 요청이 복사하지만 소유해 둔다.
    std::uint32_t logicalWidth_{1280};   ///< 장식을 뺀 논리 가로 크기.
    std::uint32_t logicalHeight_{720};   ///< 장식을 뺀 논리 세로 크기.
    std::int32_t scale_{1};              ///< 현재 출력의 정수 buffer scale. width()가 곱한다.
    bool configured_{false};             ///< 첫 configure를 받고 ack했는지.
    bool closed_{false};                 ///< 닫기 요청이나 연결 오류가 있었는지.
};

} // namespace nxt::platform::wayland
