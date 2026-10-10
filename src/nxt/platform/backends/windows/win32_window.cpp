#include <cstdint>
#include <memory>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/platform/backends/windows/win32_window.hpp>
#include <nxt/platform/window/window.hpp>
#include <string>
#include <string_view>

// per-monitor DPI는 Windows 10 1703부터 있다. 낮은 값을 선언된 SDK가
// 넘겨주면 SetProcessDpiAwarenessContext가 선언되지 않는다.
#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace {

using nxt::platform::window::WindowDesc;

/// 창 클래스 이름이다. 프로세스당 한 번만 등록한다.
constexpr wchar_t kWindowClassName[] = L"NXT_Window";

/**
 * @brief UTF-8 문자열을 UTF-16으로 변환한다.
 *
 * 창 제목은 UTF-16을 요구한다. 로캘 코드 페이지를 쓰면 변환이 조용히 깨지므로
 * CP_UTF8을 명시한다.
 */
[[nodiscard]]
std::wstring toWideString(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int length = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        NXT_LOG_WARN(Platform, Win32, "window title conversion failed, error={}", ::GetLastError());
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

/**
 * @brief 프로세스가 per-monitor DPI를 인식하도록 만든다.
 *
 * window.hpp의 width()/height()가 "OS 스케일링 영향 없음"을 지키려면 필요하다.
 * 이게 없으면 확대된 화면에서 swapchain 크기와 실제 픽셀 수가 어긋난다.
 * 실패해도 창은 뜨지만 흐릿하게 보인다.
 */
void enablePerMonitorDpiAwareness() {
    if (::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE) {
        NXT_LOG_DEBUG(Platform, Win32, "per-monitor DPI awareness V2 enabled");
        return;
    }
    NXT_LOG_WARN(Platform, Win32, "per-monitor DPI awareness unavailable, falling back to system aware");
    ::SetProcessDPIAware();
}

/**
 * @brief 창 클래스를 등록한다. 프로세스에서 한 번만 실제로 수행한다.
 *
 * 이미 등록된 경우 ERROR_CLASS_ALREADY_EXISTS는 성공으로 본다.
 */
[[nodiscard]]
bool registerWindowClass() {
    static const bool registered = [] {
        enablePerMonitorDpiAwareness();

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = ::DefWindowProcW;
        windowClass.hInstance = ::GetModuleHandleW(nullptr);
        // IDC_ARROW를 그대로 쓸 수 없다. MAKEINTRESOURCE가 LPSTR로 확장되어
        // LoadCursorW의 LPCWSTR 인자에 맞지 않는다.
        windowClass.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.lpszClassName = kWindowClassName;

        if (::RegisterClassExW(&windowClass) != 0) {
            NXT_LOG_DEBUG(Platform, Win32, "window class registered");
            return true;
        }

        const DWORD error = ::GetLastError();
        if (error == ERROR_CLASS_ALREADY_EXISTS) {
            return true;
        }

        NXT_LOG_ERROR(Platform, Win32, "RegisterClassExW failed, error={}", error);
        return false;
    }();

    return registered;
}

/// 드로잉 영역의 사각형을 반환한다. 실패하면 빈 사각형이다.
[[nodiscard]]
RECT clientRectOf(HWND hwnd) noexcept {
    RECT rect{};
    ::GetClientRect(hwnd, &rect);
    return rect;
}

/**
 * @brief 설정에 맞는 HWND를 만든다.
 *
 * 리사이즈가 가능해야 swapchain 재생성을 검증할 수 있으므로 WS_OVERLAPPEDWINDOW를 쓴다.
 */
[[nodiscard]]
HWND createNativeWindow(const WindowDesc& desc) {
    if (!registerWindowClass()) {
        return nullptr;
    }

    // 드로잉 영역이 요청한 크기가 되도록 비클라이언트 영역을 더해 준다.
    // DPI에 정확한 조절을 원하면 AdjustWindowRectExForDpi가 필요하지만,
    // 드로잉 영역 크기는 DPI와 무관하므로 swapchain에는 영향이 없다.
    RECT adjusted{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
    if (::AdjustWindowRectEx(&adjusted, WS_OVERLAPPEDWINDOW, FALSE, 0) == FALSE) {
        NXT_LOG_ERROR(Platform, Win32, "AdjustWindowRectEx failed, error={}", ::GetLastError());
        return nullptr;
    }

    const std::wstring title = toWideString(desc.title);
    HWND hwnd = ::CreateWindowExW(0, kWindowClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  adjusted.right - adjusted.left, adjusted.bottom - adjusted.top, nullptr, nullptr,
                                  ::GetModuleHandleW(nullptr), nullptr);
    if (hwnd == nullptr) {
        NXT_LOG_ERROR(Platform, Win32, "CreateWindowExW failed, error={}", ::GetLastError());
        return nullptr;
    }

    // WS_OVERLAPPEDWINDOW에 WS_VISIBLE이 들어 있지 않다. 스타일로 표시하지 않고
    // 여기서 ShowWindow를 호출하는 이유는, 위치를 잡은 뒤에 보여야 창이
    // 잘못된 위치에서 한 프레임 깜빡이지 않는다.
    ::ShowWindow(hwnd, SW_SHOW);
    return hwnd;
}

} // namespace

namespace nxt::platform::win32 {

using nxt::platform::window::NativeWindow;
using nxt::platform::window::Window;

Win32Window::Win32Window(void* hwnd) noexcept : hwnd_{hwnd} {}

Win32Window::~Win32Window() {
    // 사용자가 X 버튼으로 이미 닫았다면 hwnd_는 오래된 값이다.
    // IsWindow로 확인한 뒤에만 부수적으로 다시 파괴하지 않는다.
    const auto hwnd = static_cast<HWND>(hwnd_);
    if ((hwnd != nullptr) && (::IsWindow(hwnd) != FALSE)) {
        NXT_LOG_DEBUG(Platform, Win32, "window destroyed");
        ::DestroyWindow(hwnd);
    }
}

NativeWindow Win32Window::native() const noexcept {
    // Win32의 VkSurfaceKHR는 hinstance을 함께 요구하지만 GetModuleHandle(nullptr)로
    // 대체할 수 있다. 그래서 연결 객체를 넘길 자리가 없다.
    return NativeWindow{hwnd_, nullptr};
}

std::uint32_t Win32Window::width() const noexcept {
    const RECT rect = clientRectOf(static_cast<HWND>(hwnd_));
    return static_cast<std::uint32_t>(rect.right - rect.left);
}

std::uint32_t Win32Window::height() const noexcept {
    const RECT rect = clientRectOf(static_cast<HWND>(hwnd_));
    return static_cast<std::uint32_t>(rect.bottom - rect.top);
}

bool Win32Window::isOpen() const noexcept {
    // DefWindowProc가 WM_CLOSE를 처리해 DestroyWindow를 호출한다. 그래서 창이
    // 살아 있는지만 보면 닫힘 여부가 된다. 별도 상태 플래그가 필요 없다.
    return ::IsWindow(static_cast<HWND>(hwnd_)) != FALSE;
}

void Win32Window::pollEvents() {
    // 핸들로 걸러 이 창의 메시지만 처리한다. 창이 하나뿐인 지금은 차이가
    // 없지만, 여러 개가 되면 창 하나만 닫은 채 다른 창이 멈추는 일이 생긴다.
    MSG message{};
    int processed = 0;
    while (::PeekMessageW(&message, static_cast<HWND>(hwnd_), 0, 0, PM_REMOVE) != FALSE) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
        ++processed;
    }

    // 대부분의 프레임에는 메시지가 없다. 무조건 로그하면 조용한 프레임마다
    // 포맷 비용만 내므로, 처리된 것이 있을 때만 남긴다.
    if (processed > 0) {
        NXT_LOG_TRACE(Platform, Win32, "processed {} message(s)", processed);
    }
}

} // namespace nxt::platform::win32

std::unique_ptr<nxt::platform::window::Window>
nxt::platform::window::createWindow(const nxt::platform::window::WindowDesc& desc) {
    HWND hwnd = createNativeWindow(desc);
    if (hwnd == nullptr) {
        return nullptr;
    }
    return std::make_unique<nxt::platform::win32::Win32Window>(hwnd);
}
