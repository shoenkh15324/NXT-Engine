#pragma once

#include <nxt/platform/window/window.hpp>

namespace nxt::platform::win32 {

using nxt::platform::window::NativeWindow;
using nxt::platform::window::Window;

/**
 * @brief HWND로 구현한 창이다.
 *
 * 창 클래스 등록과 HWND 생성은 createWindow()가 수행한다. 이 클래스는 이미
 * 유효한 HWND를 소유한 상태에서 시작하므로 생성자가 실패하지 않는다.
 * 실패는 정적 팩토리 단계에서 nullptr로 표현된다.
 *
 * @note 이 헤더는 nxt_platform의 PRIVATE 파일이다. 모듈 밖에서 포함되지 않는다.
 * @note Thread safety: thread-safe하지 않다. 주 스레드에서만 조작한다.
 * @note HWND를 소유한다. 소멸할 때 DestroyWindow를 호출한다.
 */
class Win32Window final : public Window {
public:
    /**
     * @brief 이미 생성된 HWND를 소유한다.
     *
     * @param[in] hwnd 유효한 HWND. nullptr은 프로그래머 오류다.
     */
    explicit Win32Window(void* hwnd) noexcept;
    ~Win32Window() override;

    /// HWND를 소유하므로 복사와 이동을 모두 금지한다.
    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;
    Win32Window(Win32Window&&) = delete;
    Win32Window& operator=(Win32Window&&) = delete;

    /// @brief 창 시스템에 넘길 HWND를 반환한다. Win32는 연결 객체가 없다.
    NativeWindow native() const noexcept override;

    /// @brief 드로잉 영역의 가로 픽셀 수를 반환한다. 창 테두리는 포함하지 않는다.
    std::uint32_t width() const noexcept override;

    /// @brief 드로잉 영역의 세로 픽셀 수를 반환한다. 창 테두리는 포함하지 않는다.
    std::uint32_t height() const noexcept override;

    /// @brief 사용자가 창을 닫지 않았는지 여부를 반환한다.
    bool isOpen() const noexcept override;

    /// @brief 이 창의 대기 중인 메시지를 모두 처리한다.
    void pollEvents() override;

private:
    /**
     * @brief 창 핸들이다.
     *
     * windows.h를 헤더에서 제외하기 위해 불투명 포인터로 보관한다.
     * 이 클래스의 private 멤버이고 이 TU 밖에서 읽히지 않으므로 안전하다.
     */
    void* hwnd_{nullptr};
};

} // namespace nxt::platform::win32
