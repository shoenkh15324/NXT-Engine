#pragma once

#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/core/instance.hpp>

namespace nxt::graphics::rhi {

/// @brief Queue를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct QueueTag {};

/**
 * @brief GPU 작업을 제출하고 화면에 제시하는 통로이다.
 *
 * 펜스·세마포어·메모리와 달리 생성·파괴 함수가 없다. Device가 만들고 소유하며
 * Device가 사라질 때 함께 사라지기 때문이다 (rhi.md §8). 그래서 Device에서
 * 얻기만 한다.
 *
 * @note 값 핸들이다. Vulkan에서 VkQueue는 디스패치 가능한 핸들(포인터)이라
 *       ResourceHandle로 옮길 때 reinterpret_cast가 필요하다 (phase0.md A4).
 *       해석은 backend가 한다.
 * @note Phase 0은 queue family를 하나만 쓴다. 그래픽스 큐와 present 큐가 같은
 *       핸들일 수 있다 — 다중 queue family는 만들지 않는다 (rhi.md §8).
 */
using Queue = ResourceHandle<QueueTag>;

/**
 * @brief Device를 만드는 데 필요한 설정이다.
 *
 * @note Surface가 있어야 한다. 논리 디바이스를 만들 queue family를 고르기 전에
 *       그 family가 이 창에 present할 수 있는지 확인해야 하기 때문이다
 *       (rhi.md §8). 유효하지 않은 Surface를 넘기면 생성을 중단한다.
 */
struct DeviceDesc {
    /// @brief 그릴 대상 창이다. 유효한 Surface여야 한다.
    Surface surface;
};

/**
 * @brief GPU 작업을 실제로 수행하는 객체이다.
 *
 * 물리 디바이스를 골라 논리 디바이스를 만들고, 그 위에서 쓸 Queue를 준다
 * (rhi.md §8). 펜스·세마포어·메모리·커맨드 버퍼는 전부 이 객체를 첫 인자로
 * 받아 만들어지고 파괴된다 (phase0.md A4 — GPU 객체는 디바이스를 명시적으로
 * 받는다).
 *
 * @code
 * auto device = instance->createDevice({.surface = surface});
 * auto graphics = device->graphicsQueue();
 * @endcode
 *
 * @note 생성은 Instance::createDevice()가 하고, 파괴는 소멸자가 맡는다.
 *       이 객체가 사라지면 그 위에서 만든 Queue도 함께 사라진다.
 * @note Thread safety: thread-safe하지 않다. 생성과 파괴는 같은 스레드에서 한다.
 */
class Device {
public:
    virtual ~Device() = default;

    /**
     * @brief 커맨드 버퍼를 제출할 큐를 반환한다.
     *
     * @return 그래픽스 작업을 받는 큐.
     */
    [[nodiscard]]
    virtual Queue graphicsQueue() const noexcept = 0;

    /**
     * @brief 완성된 이미지를 화면에 제시할 큐를 반환한다.
     *
     * @return present를 지원하는 큐. Phase 0에서는 graphicsQueue()와 같은
     *         값일 수 있다.
     */
    [[nodiscard]]
    virtual Queue presentQueue() const noexcept = 0;
};

} // namespace nxt::graphics::rhi
