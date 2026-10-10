#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>

#if defined(_MSC_VER)
    #include <malloc.h>
#else
    #include <cstdlib>
#endif

namespace nxt::core::memory {

/**
 * @brief 기본 new가 보장하는 정렬이다.
 *
 * placement new로 객체를 만들 수 있는 정렬의 상한이다. 이보다 큰 정렬을 요구하는
 * 타입은 align_val_t를 넘기는 placement new overload가 필요한데, MSVC는 이를
 * 제공하지 않는다.
 *
 * @note alignof(std::max_align_t)로 대신하면 안 된다. MSVC에서는 이 값이 8이라
 *       alignas(16)처럼 실제로는 문제없는 타입까지 거절하게 된다.
 */
inline constexpr std::size_t kDefaultNewAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

/**
 * @brief 바이트 블록을 할당하고 해제하는 계약이다.
 *
 * 가상 함수가 아니라 concept으로 정의해 구현체가 상속 계층을 갖지 않게 한다.
 *  현재 SystemAllocator만 이 계약을 충족한다. Arena와 Pool은 각각 다른 단위로
 *  다뤄지므로 contract를 공유하지 않는다.
 *
 * @note deallocate()는 자신이 반환한 포인터만 받는다. 다른 곳에서 받은 포인터를
 *       넘기면 동작이 정의되지 않는다.
 */
template <typename T>
concept Allocator = requires(T& allocator, std::size_t size, std::size_t alignment, void* pointer) {
    { allocator.allocate(size, alignment) } -> std::same_as<void*>;
    { allocator.deallocate(pointer) } -> std::same_as<void>;
};

/**
 * @brief C 런타임에 메모리를 요청하는 기본 allocator이다.
 *
 * Arena와 Pool의 backing allocator로 쓰인다. 상태가 없어 어디에 두어도 값이 같다.
 *
 * @note Thread safety: 상태가 없으므로 thread-safe하다.
 */
class SystemAllocator {
public:
    /**
     * @brief alignment를 만족하는 size 바이트 블록을 할당한다.
     *
     * @param[in] size 요청 바이트 수. 0을 허용하며 이 경우 alignment 크기 한 블록을 할당한다.
     * @param[in] alignment 2 이상의 2의 거듭제곱이어야 한다.
     * @return 정렬된 블록의 시작 주소. 할당에 실패하면 nullptr.
     */
    [[nodiscard]]
    void* allocate(std::size_t size, std::size_t alignment) noexcept {
        // alignment가 0이면 아래 비트 연산도 통과하므로 하한을 함께 확인한다.
        NXT_ASSERT_MSG((alignment >= 2) && ((alignment & (alignment - 1)) == 0), "alignment must be a power of two");
        if (alignment == 0) {
            return nullptr;
        }

#if defined(_MSC_VER)
        // MSVC는 std::aligned_alloc을 제공하지 않는다. 따라서 크기를 정렬 배수로 올릴 필요가 없다.
        void* const block = _aligned_malloc(size, alignment);
#else
        // std::aligned_alloc는 size가 alignment의 배수여야 하므로 올려 준다.
        const std::size_t mask = alignment - 1;
        if (size > (std::numeric_limits<std::size_t>::max)() - mask) {
            NXT_LOG_TRACE(Core, Memory, "system allocation overflow: size={} alignment={}", size, alignment);
            return nullptr;
        }
        const std::size_t alignedSize = ((size == 0 ? std::size_t{1} : size) + mask) & ~mask;
        void* const block = std::aligned_alloc(alignment, alignedSize);
#endif

        // 할당 실패는 호출자가 nullptr로 받는다. 원인 정보는 여기서만 얻을 수 있다.
        if (block == nullptr) {
            NXT_LOG_TRACE(Core, Memory, "system allocation failed: size={} alignment={}", size, alignment);
        }
        return block;
    }

    /**
     * @brief allocate()가 반환한 블록을 해제한다.
     *
     * @param[in] pointer 해제할 블록의 시작 주소. nullptr이어도 안전하다.
     */
    void deallocate(void* pointer) noexcept {
#if defined(_MSC_VER)
        _aligned_free(pointer);
#else
        std::free(pointer);
#endif
    }
};

static_assert(Allocator<SystemAllocator>);

} // namespace nxt::core::memory
