#pragma once

#include <cstddef>
#include <cstdint>
#include <nxt/graphics/rhi/common/handle.hpp>

namespace nxt::graphics::rhi {

class Device;

/// @brief Memory를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct MemoryTag {};

/**
 * @brief GPU가 읽고 쓰는 메모리 할당이다.
 *
 * CPU 메모리와 다른 자원이다. nxt_core::memory::SystemAllocator는
 * _aligned_malloc / std::aligned_alloc 기반이라 **CPU 가상 메모리만** 만들고
 * GPU 버퍼에 쓸 수 없다 (rhi.md §8).
 *
 *   SystemAllocator  →  CPU 가상 메모리
 *   rhi::Memory      →  GPU 메모리
 *
 * memory type bits를 골라 할당하고, CPU가 건드려야 하는 메모리는 매핑한다.
 * SystemAllocator를 감싸도 이 둘을 얻을 수 없다. **두 계층은 대체 관계가 아니다.**
 *
 * GPU 버퍼와 이미지는 리소스와 이 메모리가 따로 있다. 리소스가 이 메모리에
 * 묶여야 쓸 수 있고, 이미지라면 여기에 ImageView가 더 있어야 렌더 타겟이 된다.
 * 그 연결은 backend가 맡는다.
 *
 * @note 소유권이 없다. 파괴는 backend가 결정한 시점에 일어난다 (rhi.md §9).
 * @note Thread safety: thread-safe하지 않다. 한 소유자가 단독으로 접근해야 한다.
 */
using Memory = ResourceHandle<MemoryTag>;

/**
 * @brief 메모리 할당이 만족해야 하는 성질을 나타내는 비트 집합이다.
 *
 * VkMemoryPropertyFlagBits를 옮긴 것이다. OR로 여러 개를 요구한다.
 * 요구한 성질을 모두 가진 타입을 GPU가 찾지 못하면 할당이 실패한다 — 어떤
 * GPU는 HostVisible과 DeviceLocal을 동시에 주지 않는다.
 */
enum class MemoryProperty : std::uint32_t {
    /// @brief 성질 요구가 없다. 가장 느리지만 가장 호환된다.
    None = 0,
    /// @brief GPU 전용. 가장 빠르다. CPU가 접근할 수 없다.
    DeviceLocal = 1u << 0,
    /// @brief CPU가 접근할 수 있다. 접근 속도가 느리다.
    HostVisible = 1u << 1,
    /// @brief CPU 읽기가 빠르다. HostVisible과 함께 쓴다.
    HostCached = 1u << 2,
    /// @brief CPU 쓰기가 자동으로 GPU에 반영된다. 명시적 flush가 필요 없다.
    HostCoherent = 1u << 3,
};

/// @brief 두 성질을 합친다.
[[nodiscard]]
constexpr MemoryProperty operator|(MemoryProperty lhs, MemoryProperty rhs) noexcept {
    return static_cast<MemoryProperty>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

/// @brief 기존 성질에 더한다.
constexpr MemoryProperty& operator|=(MemoryProperty& lhs, MemoryProperty rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

/**
 * @brief GPU 메모리를 할당한다.
 *
 * required의 성질을 모두 가진 메모리 타입을 찾아 그 크기만큼 할당한다.
 * 찾지 못하면 종료한다 — GPU 오류는 복구 불가능하기 때문이다 (rhi.md §7).
 *
 * @note Phase 0에서 할당은 한두 번뿐이다. allocator 계층은 만들지 않는다.
 *       할당이 많아지는 Phase 2에서 VMA를 검토한다.
 *
 * @param[in] device 할당에 쓸 디바이스.
 * @param[in] size 할당할 바이트 수. 0은 실패한다.
 * @param[in] required 필요한 메모리 성질.
 *
 * @return 할당된 메모리.
 */
[[nodiscard]]
Memory allocateMemory(const Device& device, std::uint64_t size, MemoryProperty required);

/**
 * @brief GPU 메모리를 해제한다.
 *
 * 매핑이 남아 있으면 실패한다. 먼저 unmapMemory()를 호출해야 한다.
 * GPU가 아직 쓰고 있을 수 있다 — 리소스 파괴와 같은 규칙이다. 확인하기 어려운
 * 메모리는 resource_deletion의 지연 파괴 경로를 쓴다 (frame.md §9).
 *
 * @param[in] device 해제에 쓸 디바이스.
 * @param[in] memory 해제할 메모리.
 */
void freeMemory(const Device& device, const Memory& memory);

/**
 * @brief GPU 메모리를 CPU 주소 공간에 매핑한다.
 *
 * 매핑한 동안 CPU가 직접 읽고 쓸 수 있다. 쓰기 내용은 HostCoherent 성질을
 * 요구하면 자동으로 GPU에 반영된다. 그 성질이 없으면 GPU에 반영되지 않으므로
 * 다른 수단이 필요하다.
 *
 * @note HostVisible이 아닌 메모리는 매핑에 실패한다.
 * @note 매핑한 구간 밖의 주소는 쓸 수 없다. 범위를 벗어나면 세그폴트가 나거나
 *       UB가 된다.
 *
 * @param[in] device 매핑에 쓸 디바이스.
 * @param[in] memory 매핑할 메모리.
 * @param[in] offset 메모리 안에서 매핑을 시작할 바이트 오프셋.
 * @param[in] size 매핑할 바이트 수.
 *
 * @return CPU가 접근할 수 있는 주소. 실패하면 nullptr.
 */
[[nodiscard]]
void* mapMemory(const Device& device, const Memory& memory, std::uint64_t offset, std::size_t size);

/**
 * @brief 매핑을 해제한다.
 *
 * 해제하면 얻었던 주소는 더 이상 쓸 수 없다. freeMemory()보다 먼저 호출해야 한다.
 * 매핑을 해제하는 것과 메모리를 해제하는 것은 다르다 — 메모리는 그대로 살아 있다.
 *
 * @param[in] device 해제에 쓸 디바이스.
 * @param[in] memory 해제할 매핑의 대상 메모리.
 */
void unmapMemory(const Device& device, const Memory& memory);

} // namespace nxt::graphics::rhi
