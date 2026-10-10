#pragma once

#include <cstddef>
#include <limits>
#include <new>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/memory/allocator.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace nxt::core::memory {

/**
 * @brief 개수를 미리 정해 둔 슬롯에서 T를 반복적으로 생성하고 파괴하는 풀이다.
 *
 * Arena와 달리 객체 수명을 풀 책임으로 한다. allocate()가 생성자를 호출하고,
 * deallocate()가 소멸자를 호출한다. 수명이 명시적으로 필요한 타입은 이쪽을 쓴다.
 *
 * 슬롯 크기가 고정이라 비어 있는 슬롯을 찾을 때 선형 탐색이 필요 없다.
 *
 * @note Thread safety: thread-safe하지 않다. 단독 소유자가 접근해야 한다.
 * @note Allocator concept을 만족하지 않는다. 바이트 크기가 아니라 T의 슬롯 수를 다루므로
 *       Arena나 SystemAllocator와 바꿔 쓸 수 없다.
 */
template <typename T>
class Pool {
    static_assert(!std::is_void_v<T> && !std::is_array_v<T> && !std::is_reference_v<T> && !std::is_const_v<T>,
                  "Pool<T> requires a non-const complete object type");

    // 블록은 alignof(T)만큼 정렬해 할당하지만, 과도 정렬 타입의 객체 생성에는 placement new의
    // align_val_t overload가 필요하다. MSVC는 이 overload를 제공하지 않는다.
    // 조용히 어긋난 정렬로 객체를 만드는 것보다 컴파일 에러로 거절한다.
    static_assert(alignof(T) <= kDefaultNewAlignment, "Pool<T> does not support types that require extended alignment");

public:
    /**
     * @brief slotCount개의 슬롯을 미리 확보한다.
     *
     * 생성 시 슬롯 블록과 free list에 1회씩 할당이 발생한다. 이후 allocate() /
     * deallocate() 경로에서는 추가 할당이 없다.
     *
     * @param[in] slotCount 확보할 슬롯 수.
     */
    explicit Pool(std::size_t slotCount)
        : slotCount_{slotCount <= ((std::numeric_limits<std::size_t>::max)() / sizeof(T)) ? slotCount : 0},
          base_{slotCount_ == 0
                    ? nullptr
                    : static_cast<std::byte*>(SystemAllocator{}.allocate(slotCount_ * sizeof(T), alignof(T)))} {
        NXT_ASSERT_MSG((slotCount_ == 0) || (base_ != nullptr), "Pool failed to allocate its slot block");
        // 역순으로 넣어 pop_back()이 0번 슬롯부터 차례로 꺼내게 한다. 슬롯이 앞에서부터
        // 채워져야 연속 접근이 유지된다.
        freeList_.reserve(slotCount_);
        for (std::size_t i = slotCount_; i > 0; --i) {
            freeList_.push_back(base_ + (i - 1) * sizeof(T));
        }
    }

    /**
     * @brief 남아 있는 모든 객체의 파괴를 요구하고 슬롯 블록을 해제한다.
     *
     * 살아 있는 객체가 남아 있으면 파괴 경로가 없으므로 디버그에서 즉시 드러낸다.
     * Release에서는 검사가 생략되어 남은 객체의 자원이 회수되지 않는다.
     */
    ~Pool() {
        NXT_ASSERT_MSG(liveCount_ == 0, "Pool destroyed while objects are still alive");
        SystemAllocator{}.deallocate(base_);
    }

    /// 슬롯을 소유하므로 복사와 이동을 모두 금지한다.
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    /**
     * @brief 빈 슬롯 하나에 T를 값 초기화로 생성한다.
     *
     * 값 초기화로 만들기 때문에 재사용되는 슬롯에 이전 객체의 데이터가 남지 않는다.
     * 따라서 T는 기본 생성 가능해야 한다.
     *
     * @return 생성된 객체의 주소. 슬롯이 고갈되면 nullptr.
     */
    [[nodiscard]]
    T* allocate() {
        // T를 명시하면 Args가 {T}로 고정되어 시그니처가 달라진다. 생성자 인자가 없으므로
        // 빈 패킹으로 호출한다.
        return allocateConstruct<>();
    }

    /**
     * @brief 빈 슬롯 하나에 인자로 T를 생성한다.
     *
     * 기본 생성자가 없는 타입은 이 경로만 쓴다.
     *
     * @param[in] args T의 생성자에 전달할 인자.
     * @return 생성된 객체의 주소. 슬롯이 고갈되면 nullptr.
     */
    template <typename... Args>
    [[nodiscard]]
    T* allocateConstruct(Args&&... args) {
        std::byte* const slot = popSlot();
        if (slot == nullptr) {
            return nullptr;
        }

        // 블록은 alignof(T)만큼 정렬되어 할당되지만, 과도 정렬 타입은 placement new에
        // align_val_t를 함께 넘겨야 한다. MSVC는 해당 overload를 제공하지 않아
        // 컴파일 타임에 거부한다.
        return ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
    }

    /**
     * @brief 객체를 소멸시키고 슬롯을 반환한다.
     *
     * 이 풀에서 allocate()로 얻은 주소만 넘긴다.
     *
     * @param[in] object 해제할 객체의 주소.
     */
    void deallocate(T* object) noexcept {
        NXT_ASSERT_MSG(object != nullptr, "deallocate() received nullptr");
        NXT_ASSERT_MSG(atSlotStart(object), "deallocate() received a pointer that is not a slot of this pool");

        object->~T();
        freeList_.push_back(static_cast<std::byte*>(static_cast<void*>(object)));
        --liveCount_;
    }

    /**
     * @brief 주어진 포인터가 이 풀의 슬롯 블록 안에 있는지 여부를 반환한다.
     *
     * 잘못된 allocator로 해제하려 할 때 NXT_ASSERT의 조건으로 쓴다.
     */
    [[nodiscard]]
    bool owns(const void* pointer) const noexcept {
        if (base_ == nullptr) {
            return false;
        }
        const auto* const bytes = static_cast<const std::byte*>(pointer);
        return (bytes >= base_) && (bytes < (base_ + (slotCount_ * sizeof(T))));
    }

    /// 확보한 전체 슬롯 수를 반환한다.
    [[nodiscard]]
    std::size_t capacity() const noexcept {
        return slotCount_;
    }

    /// 지금 즉시 사용할 수 있는 슬롯 수를 반환한다.
    [[nodiscard]]
    std::size_t available() const noexcept {
        return freeList_.size();
    }

    /// 생성된 채 파괴되지 않은 객체 수를 반환한다.
    [[nodiscard]]
    std::size_t liveCount() const noexcept {
        return liveCount_;
    }

private:
    /// 빈 슬롯 하나를 꺼내 바이트 주소를 반환한다. 고갈이면 nullptr.
    [[nodiscard]]
    std::byte* popSlot() {
        if (freeList_.empty()) {
            return nullptr;
        }
        std::byte* const slot = freeList_.back();
        freeList_.pop_back();
        ++liveCount_;
        return slot;
    }

    /// 포인터가 슬롯의 시작을 가리키는지 여부를 반환한다.
    [[nodiscard]]
    bool atSlotStart(const void* pointer) const noexcept {
        if (!owns(pointer)) {
            return false;
        }
        const auto* const bytes = static_cast<const std::byte*>(pointer);
        return (static_cast<std::size_t>(bytes - base_) % sizeof(T)) == 0;
    }

    std::size_t slotCount_;            /// 확보한 슬롯 수.
    std::byte* base_;                  /// 슬롯 블록의 시작 주소. 비어 있으면 nullptr.
    std::size_t liveCount_{0};         /// 사용 중인 슬롯 수.
    std::vector<std::byte*> freeList_; /// 비어 있는 슬롯의 시작 주소 목록이다.
};

} // namespace nxt::core::memory
