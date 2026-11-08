#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/resource/memory.hpp>

namespace nxt::graphics::rhi {

class Device;

/// @brief Buffer를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct BufferTag {};

/**
 * @brief GPU가 읽는 선형 메모리 블록이다.
 *
 * 정점 · 인덱스 · 균일 상수의 원본이 되고, 전송의 출발점이나 도착점이 된다.
 * VkBuffer는 메모리를 갖지 않으므로 그것만 만들어서는 쓸 수 없다. createBuffer()가
 * 요구 사항을 조회해 메모리까지 할당하고 묶는다.
 *
 * @note 값 핸들이다. vk 핸들 외에 자체 상태가 없다 (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권이 없다. createBuffer()가 만들고 destroyBuffer()가 해제한다.
 */
using Buffer = ResourceHandle<BufferTag>;

/**
 * @brief buffer를 만드는 데 필요한 설정이다.
 *
 * 크기와 용도는 호출자가 정하고, 메모리 성질만 고르면 나머지는 backend가 정한다.
 * 메모리 크기와 type은 드라이버가 요구 사항으로 알려 주므로 여기에 넣지 않는다.
 */
struct BufferDesc {
    /// @brief 바이트 단위 크기이다. 0이면 안 된다.
    std::uint64_t size = 0;

    /// @brief 버퍼를 어떤 용도로 쓸지 나타낸다. 하나 이상 켜야 한다.
    BufferUsage usage = BufferUsage::None;

    /**
     * @brief 버퍼가 묶일 메모리의 성질이다.
     *
     * CPU가 직접 채우는 버퍼는 HostVisible | HostCoherent가 필요하다. DeviceLocal만
     * 요구하면 CPU가 접근할 수 없으므로 스테이징 버퍼에서 복사해야 한다.
     *
     * @note 어떤 GPU는 두 성질을 동시에 주지 않는다. 그때는 할당이 실패한다
     *       (memory.hpp).
     */
    MemoryProperty memoryProperty = MemoryProperty::HostVisible | MemoryProperty::HostCoherent;
};

/**
 * @brief 만들어진 버퍼와 그 버퍼가 묶인 메모리이다.
 *
 * 둘은 따로 만들 수 없다. 버퍼의 요구 사항을 알아야 메모리 크기와 type을 정할 수
 * 있고, 그 값은 backend만 조회할 수 있기 때문이다. 그래서 생성과 파괴를 한 쌍으로
 * 묶었다.
 *
 * @note CPU가 채우려면 memory를 mapMemory()에 넘긴다. 매핑은 buffer가 아니라
 *       memory의 성질이다.
 * @note destroyBuffer()가 버퍼를 먼저 지우고 메모리를 해제한다. 순서를 호출자에게
 *       맡기지 않는다.
 */
struct BufferAllocation {
    /// @brief 만들어진 버퍼이다.
    Buffer buffer;

    /// @brief 그 버퍼가 묶인 메모리이다.
    Memory memory;
};

/**
 * @brief buffer를 만들고 메모리를 할당해 묶는다.
 *
 * 크기와 memory type은 드라이버가 정한다. 정렬 때문에 요구 크기가 desc.size보다
 * 클 수 있으므로 할당은 요구 사항의 크기로 한다.
 *
 * @param[in] device buffer를 만들 디바이스.
 * @param[in] desc   크기 · 용도 · 메모리 성질.
 *
 * @return 만들어진 버퍼와 메모리.
 */
[[nodiscard]]
BufferAllocation createBuffer(const Device& device, const BufferDesc& desc);

/**
 * @brief buffer를 지우고 그 메모리를 해제한다.
 *
 * 버퍼를 먼저 지운다. 메모리를 먼저 해제하면 버퍼가 죽은 메모리를 가리킨다.
 *
 * @note 매핑이 남아 있으면 호출자가 먼저 unmapMemory()를 불러야 한다. 매핑된
 *       메모리는 해제할 수 없다. destroyBuffer()가 대신 unmap하지 않는 이유는
 *       매핑 여부를 알 수 없고, 매핑되지 않은 메모리에 vkUnmapMemory를 부르면
 *       그쪽이 잘못이 되기 때문이다.
 * @note GPU가 아직 이 버퍼를 읽고 있을 수 있다. 프레임 경계에서 부르거나
 *       resource_deletion의 지연 파괴 경로를 쓴다 (frame.md §9).
 * @note 매핑이 남아 있으면 호출자가 먼저 unmapMemory()를 불러야 한다. 매핑된
 *       메모리는 해제할 수 없다.
 *
 * @param[in] device     buffer를 만든 디바이스.
 * @param[in] allocation 해제할 버퍼와 메모리.
 */
void destroyBuffer(const Device& device, const BufferAllocation& allocation);

} // namespace nxt::graphics::rhi
