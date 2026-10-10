#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace nxt::platform::window {

/**
 * @brief 창 시스템에 넘겨야 하는 원시 핸들 묶음이다.
 *
 * 창 식별자 하나만으로는 surface를 만들 수 없는 창 시스템이 있다.
 * Win32는 { hinstance, hwnd }를 요구하지만 hinstance는 GetModuleHandle(nullptr)로
 * 대체할 수 있어 필드를 늘리지 않는다. X11과 Wayland는 연결 객체까지 요구한다.
 * 그 차이를 여기서 흡수한다. 인터페이스를 깨지 않으면서 Linux 백엔드를 붙이려면
 * 이 필드가 먼저 있어야 한다.
 *
 * @note Window가 살아 있는 동안에만 유효하다. 창이 닫히면 dangling이 된다.
 * @note 포인터 크기로 승격하지 않은 XID가 handle에 들어가는 경우, 포인터 크기로
 *       변환해 담는다.
 */
struct NativeWindow {
    void* handle{nullptr};  ///< HWND | xcb_window_t 승격값 | wl_surface*
    void* display{nullptr}; ///< 연결 객체. Win32는 항상 nullptr.
};

/**
 * @brief 창 생성에 필요한 설정이다.
 *
 * 모든 필드에 기본값이 있으므로 뒤에 필드를 추가해도 기존 호출은 그대로 컴파일된다.
 *
 * @note title은 호출 중 유효하면 충분하다. 백엔드는 생성 시 복사한다.
 */
struct WindowDesc {
    std::uint32_t width{1280};
    std::uint32_t height{720};
    std::string_view title{"NXT"};
};

/**
 * @brief OS가 만든 창을 사용하기 위한 인터페이스다.
 *
 * 구현은 platform/backends/<os>/ 에 있고 createWindow()로만 얻을 수 있다.
 * backend 헤더를 직접 include하지 않는다. 이 경계를 지키지 않으면 호출부가
 * 창 시스템에 종속되어 계층이 무너진다.
 *
 * @note Thread safety: thread-safe하지 않다. 생성·조작·파괴는 같은 스레드에서
 *       해야 하며 그 스레드는 하나여야 한다. 관례상 주 스레드다.
 * @note 소유권은 호출부에 있다. 창은 main thread에서 매 프레임 pollEvents()를
 *       호출하며, 렌더링 스레드가 이 객체를 건드리면 안 된다.
 */
class Window {
public:
    virtual ~Window() = default;

    /**
     * @brief 창 시스템에 넘길 원시 핸들을 반환한다.
     *
     * graphics 계층이 surface를 만들 때만 쓴다. 창 크기가 바뀌어도 핸들 자체는
     * 유지되지만, 창이 닫히면 다시 얻어도 사용할 수 없다.
     */
    [[nodiscard]]
    virtual NativeWindow native() const noexcept = 0;

    /**
     * @brief 창이 그려지는 실제 가로 픽셀 수를 반환한다.
     *
     * OS 스케일링의 영향을 받지 않는 값이다. swapchain 크기를 정할 때 그대로 쓴다.
     * 따라서 창이 얼마나 크게 보이는지 알 수 없다. 그 값이 필요하다면 앞으로
     * 별도 메서드로 추가한다.
     */
    [[nodiscard]]
    virtual std::uint32_t width() const noexcept = 0;

    /// @brief 창이 그려지는 실제 세로 픽셀 수를 반환한다.
    [[nodiscard]]
    virtual std::uint32_t height() const noexcept = 0;

    /// @brief 사용자가 창을 닫지 않았는지 여부를 반환한다.
    ///
    /// 닫기 요청은 pollEvents()를 호출한 뒤에 반영된다.
    [[nodiscard]]
    virtual bool isOpen() const noexcept = 0;

    /**
     * @brief 대기 중인 OS 이벤트를 처리한다.
     *
     * 닫기 요청, 이동, 크기 변화 같은 상태가 반영된다. 프레임마다 한 번 이상
     * 호출한다. 호출하지 않으면 이벤트 큐가 쌓이고 창이 응답하지 않는다.
     */
    virtual void pollEvents() = 0;
};

/**
 * @brief 현재 빌드에 포함된 백엔드로 창을 만든다.
 *
 * 어떤 백엔드가 쓰이는지는 platform/backends/CMakeLists.txt가 결정한다.
 * 이 함수에는 분기가 없다. 선택된 백엔드만 컴파일되어 링크가 심볼을 해석한다.
 * 따라서 실행 중 백엔드를 바꿀 수 없고, 이 인터페이스도 플랫폼마다 갈라지지 않는다.
 *
 * @param[in] desc 창 생성 설정.
 * @return 생성된 창. 실패하면 nullptr이며 원인은 로그로 남긴다.
 *
 * @note 예외를 던지지 않는다.
 */
[[nodiscard]]
std::unique_ptr<Window> createWindow(const WindowDesc& desc);

} // namespace nxt::platform::window
