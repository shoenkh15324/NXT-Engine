#pragma once

#include <cstddef>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/memory/allocator.hpp>
#include <type_traits>

namespace nxt::core::memory {

/**
 * @brief 생성 시 확보한 블록 하나에서만 메모리를 잘라 쓰는 bump allocator이다.
 *
 * 개별 해제 API가 없다. 해제 대신 reset()으로 전체를 한 번에 되돌리며,
 * 그때 블록에 만들어진 객체의 소멸자도 호출되지 않는다. 수명이 프레임 단위로
 * 묶이는 임시 데이터에 쓴다.
 *
 * 메모리는 원시 바이트로만 해석된다. 생성/파괴는 호출부가 placement new로 직접 한다.
 *
 * @note Thread safety: thread-safe하지 않다. 단독 소유자가 접근해야 한다.
 * @note Allocator concept을 만족하지 않는다. deallocate()가 없기 때문에,
 *       다른 allocator와 바꿔 쓸 수 없다.
 */
class Arena {
public:
    /// 기본 정렬 요구량이다. 블록 할당 정렬과 같다.
    inline static constexpr std::size_t kDefaultAlignment = kDefaultNewAlignment;

    /**
     * @brief capacity 바이트의 블록을 확보한다.
     *
     * 할당에 실패하면 capacity()가 0인 빈 arena가 된다. 이 상태에서 allocate()는
     * 항상 nullptr을 반환한다.
     *
     * @param[in] capacity 미리 확보할 바이트 수.
     */
    explicit Arena(std::size_t capacity) noexcept
        : capacity_{capacity},
          base_{capacity == 0 ? nullptr
                              : static_cast<std::byte*>(SystemAllocator{}.allocate(capacity, kDefaultAlignment))},
          offset_{0} {}

    ~Arena() {
        SystemAllocator{}.deallocate(base_);
    }

    /// 블록을 소유하므로 복사할 수 없다.
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    /**
     * @brief 블록 소유권을 가져간다.
     *
     * 이동된 쪽은 capacity()가 0인 빈 arena가 된다.
     */
    Arena(Arena&& other) noexcept : capacity_{other.capacity_}, base_{other.base_}, offset_{other.offset_} {
        other.release();
    }

    /// 기존 블록을 해제하고 other's 블록을 가져온다.
    Arena& operator=(Arena&& other) noexcept {
        if (this != &other) {
            SystemAllocator{}.deallocate(base_);
            capacity_ = other.capacity_;
            base_ = other.base_;
            offset_ = other.offset_;
            other.release();
        }
        return *this;
    }

    /**
     * @brief size 바이트를 잘라 쓰고, 그 시작 주소를 반환한다.
     *
     * 반환된 블록은 Arena가 소유한다. 개별 해제 방법은 없고 reset()까지 유효하다.
     *
     * @param[in] size 요청 바이트 수.
     * @param[in] alignment kDefaultAlignment 이하여야 하는 2의 거듭제곱.
     * @return 정렬된 블록의 시작 주소. 남은 용량이 부족하면 nullptr.
     */
    [[nodiscard]]
    void* allocate(std::size_t size, std::size_t alignment = kDefaultAlignment) noexcept {
        NXT_ASSERT_MSG((alignment & (alignment - 1)) == 0, "alignment must be a power of two");
        NXT_ASSERT_MSG(alignment <= kDefaultAlignment, "alignment exceeds arena block alignment");

        if ((alignment == 0) || (base_ == nullptr)) {
            return nullptr;
        }

        // offset_ + mask를 먼저 계산하면 unsigned로 넘칠 수 있다. 넘친 경우 정렬 결과가
        // offset_보다 작아지므로 그 조건으로 잡아낸다.
        const std::size_t alignedOffset = ((offset_ + (alignment - 1)) & ~(alignment - 1));
        if ((alignedOffset < offset_) || (alignedOffset > capacity_)) {
            return nullptr;
        }

        if (size > (capacity_ - alignedOffset)) {
            return nullptr;
        }

        void* const block = base_ + alignedOffset;
        offset_ = alignedOffset + size;
        return block;
    }

    template <typename T>
    [[nodiscard]]
    T* allocateObject() noexcept {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Arena does not run destructors; use Pool<T> for types that need destruction");
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }

    /**
     * @brief 사용 중인 바이트를 0으로 되돌린다.
     *
     * 만들어진 객체의 소멸자를 호출하지 않는다. trivially destructible이거나
     * 소멸 책임이 없는 데이터에만 쓴다.
     */
    void reset() noexcept {
        offset_ = 0;
    }

    /// 확보한 전체 바이트 수를 반환한다.
    [[nodiscard]]
    std::size_t capacity() const noexcept {
        return capacity_;
    }

    /// reset() 없이 이미 잘라 쓴 바이트 수를 반환한다.
    [[nodiscard]]
    std::size_t bytesUsed() const noexcept {
        return offset_;
    }

    /// 더 할당할 수 있는 바이트 수를 반환한다.
    [[nodiscard]]
    std::size_t remaining() const noexcept {
        return capacity_ - offset_;
    }

    /// 더 이상 1바이트도 할당할 수 없는지 여부를 반환한다.
    [[nodiscard]]
    bool full() const noexcept {
        return (offset_ >= capacity_);
    }

private:
    /// 블록을 OS에 반환하고 빈 상태로 되돌린다.
    void release() noexcept {
        capacity_ = 0;
        base_ = nullptr;
        offset_ = 0;
    }

    std::size_t capacity_; /// 확보한 블록 전체의 크기
    std::byte* base_;      /// 블록의 시작 주소. 빈 arena면 nullptr.
    std::size_t offset_;   /// 다음 배치가 시작될 오프셋.
};

} // namespace nxt::core::memory
