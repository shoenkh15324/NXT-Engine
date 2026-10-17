#pragma once

#include <memory>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/platform/window/window.hpp>

namespace nxt::graphics::rhi {

struct DeviceDesc;
class Device;

/// @brief Surface를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct SurfaceTag {};

/**
 * @brief 창이 그린 결과를 화면에 내보낼 수 있게 해 주는 대상이다.
 *
 * 창은 platform이 만들고 HWND까지만 넘긴다. GPU가 그 창 위에 그릴 수 있게 하는
 * 일은 graphics의 몫이다 (phase0.md A3). 스왑체인은 이 대상 위에 만들어진다.
 *
 * @note 값 핸들이다. vk 핸들 외에 자체 상태가 없고, format이나 imageCount처럼
 *       Swapchain이 대신 들고 있어야 할 정보도 없다. 그래서 추상 클래스가 아니라
 *       Fence·Semaphore와 같은 값 핸들로 둔다 (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권이 없다. Instance::createSurface()로 만들고
 *       Instance::destroySurface()로 파괴한다. 창보다 먼저 파괴해야 한다 — 창이
 *       사라지면 그 위에 만든 대상도 더 이상 화면에 제시할 수 없다.
 */
using Surface = ResourceHandle<SurfaceTag>;

/**
 * @brief GPU와 대화하는 최상위 객체이다.
 *
 * 지원 extension을 확인하고, 창 위에 그릴 Surface를 만들고, 실제 GPU 작업을
 * 수행할 Device를 만든다 (rhi.md §8).
 *
 * @code
 * auto instance = rhi::Instance::create();
 * auto surface = instance->createSurface(window->native());
 * auto device = instance->createDevice(desc);
 * @endcode
 *
 * @note 생성은 create()만 하고, 파괴는 소멸자가 맡는다. Surface와 Device는 이
 *       객체보다 오래 살 수 없다.
 * @note Thread safety: thread-safe하지 않다. 생성과 파괴는 같은 스레드에서 한다.
 * @note create()는 rhi/에 선언만 있고 backend가 정의한다. 그래서 factory를 RHI
 *       밖에서 작성할 수 없다 — 규칙이 관례가 아니라 빌드 구조로 강제된다.
 */
class Instance {
public:
    virtual ~Instance() = default;

    /**
     * @brief Vulkan 인스턴스를 만든다.
     *
     * 지원 instance extension을 확인하고 필요한 것만 켠다. 실패하면 원인을
     * 로그로 남기고 종료한다 — GPU 오류는 복구 불가능하다 (rhi.md §7).
     *
     * @return 만들어진 인스턴스.
     */
    [[nodiscard]]
    static std::unique_ptr<Instance> create();

    /**
     * @brief 창 위에 그릴 Surface를 만든다.
     *
     * @param[in] window 창의 원시 핸들. Win32는 HWND이다.
     * @return 만들어진 Surface.
     */
    [[nodiscard]]
    virtual Surface createSurface(const platform::window::NativeWindow& window) = 0;

    /**
     * @brief Surface를 파괴한다.
     *
     * 스왑체인이 이 Surface를 참조하므로 스왑체인을 먼저 파괴한 뒤 호출한다
     * (frame.md §9). 창보다 먼저 파괴해야 한다.
     *
     * @param[in] surface 파괴할 Surface.
     */
    virtual void destroySurface(const Surface& surface) = 0;

    /**
     * @brief GPU 작업을 수행할 Device를 만든다.
     *
     * Device를 만들 수 있는 유일한 지점이다 (phase0.md A4 — Instance가 Device를
     * 만드는 유일한 지점이기 때문이다).
     *
     * @param[in] desc 물리 디바이스 선택과 논리 디바이스 생성 설정.
     * @return 만들어진 Device.
     */
    [[nodiscard]]
    virtual std::unique_ptr<Device> createDevice(const DeviceDesc& desc) = 0;
};

} // namespace nxt::graphics::rhi
