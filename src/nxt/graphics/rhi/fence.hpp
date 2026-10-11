#pragma once

#include <nxt/graphics/rhi/handle.hpp>

namespace nxt::graphics::rhi {

class Device;

/// @brief Fence를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct FenceTag {};

/**
 * @brief CPU가 GPU 작업의 완료를 기다리는 동기화 객체이다.
 *
 * 신호된 상태로 만들어진다. submit이 신호하고, CPU가 기다린다. 한 번 신호되면
 * 다시 비신호 상태로 돌아가지 않으므로 재사용하려면 먼저 resetFence()를
 * 호출해야 한다.
 *
 * NXT가 쓰는 곳은 세 군데다 (frame.md §8).
 *
 *   프레임 페이싱    GPU가 너무 앞서가지 못하게 매 프레임 기다린다
 *   지연 파괴        리소스를 파괴해도 안전한 시점을 정한다 (frame.md §9)
 *   리사이즈 정리    swapchain을 재생성하기 전에 GPU 사용이 끝나기를 기다린다
 *
 * @note Semaphore와 다르다 — Fence는 CPU가 기다리고 재사용 전에 reset이 필요하다.
 *
 * @note 소유권이 없다. 파괴는 backend가 결정한 시점에 일어난다 (rhi.md §9).
 */
using Fence = ResourceHandle<FenceTag>;

/**
 * @brief 신호된 상태의 fence를 만든다.
 *
 * 생성 직후 신호되어 있다. 제출 전에 대기지 않게 하기 위해서다.
 *
 * @param[in] device fence를 만들 디바이스.
 *
 * @return 생성된 fence.
 */
[[nodiscard]]
Fence createFence(const Device& device);

/**
 * @brief fence를 해제한다.
 *
 * GPU가 아직 이 fence를 기다리고 있을 수 있다. 확인하기 어려운 리소스는
 * resource_deletion의 지연 파괴 경로를 쓴다 (frame.md §9).
 *
 * @param[in] device 해제에 쓸 디바이스.
 * @param[in] fence 해제할 fence.
 */
void destroyFence(const Device& device, const Fence& fence);

/**
 * @brief fence가 신호될 때까지 CPU가 기다린다.
 *
 * 기다리지 않고 상태만 보려면 isSignaled()를 쓴다.
 *
 * @note 기다린 뒤에야 "이 리소스는 더 이상 GPU가 쓰지 않는다"고 말할 수
 *       있다. 순서를 바꾸면 파괴 직전에 GPU가 여전히 쓰는 리소스를 지운다
 *       (frame.md §9).
 *
 * @param[in] device 대기에 쓸 디바이스.
 * @param[in] fence 기다릴 fence.
 */
void waitFence(const Device& device, const Fence& fence);

/**
 * @brief fence를 비신호 상태로 되돌린다.
 *
 * 다음 제출에 재사용하기 전에 반드시 호출한다. 호출하지 않으면 이미 신호된
 * 상태라 waitFence가 즉시 반환하고 CPU가 GPU를 기다리지 못한다.
 *
 * @param[in] device 되돌릴 디바이스.
 * @param[in] fence 되돌릴 fence.
 */
void resetFence(const Device& device, const Fence& fence);

/**
 * @brief fence가 현재 신호되어 있는지 기다리지 않고 반환한다.
 *
 * 지연 파괴 목록에서 "이 프레임의 리소스를 이제 파괴해도 되는가"를 판단하는
 * 수단이다 (frame.md §9).
 *
 * @param[in] device 확인할 디바이스.
 * @param[in] fence 확인할 fence.
 *
 * @return 신호되어 있으면 true.
 */
[[nodiscard]]
bool isSignaled(const Device& device, const Fence& fence);

} // namespace nxt::graphics::rhi
