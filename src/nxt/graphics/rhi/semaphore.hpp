#pragma once

#include <nxt/graphics/rhi/handle.hpp>

namespace nxt::graphics::rhi {

/// @brief Semaphore를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct SemaphoreTag {};

class Device;

/**
 * @brief GPU 작업이 끝나면 신호를 보내는 동기화 객체이다.
 *
 * 한 GPU 작업의 완료가 다른 GPU 작업의 시작을 지연시킨다. CPU는 semaphore를
 * 기다릴 수 없다. CPU가 기다리려면 Fence를 쓴다.
 *
 * NXT가 쓰는 곳은 두 군데다 (frame.md §5).
 *
 *   acquire semaphore acquire()가 신호하고 submit이 기다린다
 *   submit semaphore    submit이 신호하고 present가 기다린다
 *
 * 두 배열의 인덱스 체계가 다르다. acquire는 framesInFlight로, submit은
 * swapchainImageCount로 색인한다. vkQueuePresentKHR가 semaphore를 신호하지
 * 않기 때문이다.
 *
 * @note Semaphore에 단독 대기 연산이 없다. Vulkan의 semaphore 대기는 항상
 *       submit과 present의 wait 목록에 넣어서 한다. device 전체를 기다리는
 *       일은 fence를 쓴다 (frame.md §6).
 *
 * @note 소유권이 없다. 파괴는 backend가 결정한 시점에 일어난다 (rhi.md §9).
 */
using Semaphore = ResourceHandle<SemaphoreTag>;

/**
 * @brief 비신호 상태의 semaphore를 만든다.
 *
 * 생성 직후에는 신호되지 않았다. 누군가 신호해야 GPU 작업이 진행된다.
 *
 * @param[in] device semaphore를 만들 디바이스.
 *
 * @return 생성된 semaphore.
 */
[[nodiscard]]
Semaphore createSemaphore(const Device& device);

/**
 * @brief semaphore를 해제한다.
 *
 * GPU가 아직 이 semaphore를 기다리고 있을 수 있다. 확인하기 어려운 리소스는
 * resource_deletion의 지연 파괴 경로를 쓴다 (frame.md §9).
 *
 * @param[in] device 해제에 쓸 디바이스.
 * @param[in] semaphore 해제할 semaphore.
 */
void destroySemaphore(const Device& device, const Semaphore& semaphore);

} // namespace nxt::graphics::rhi
