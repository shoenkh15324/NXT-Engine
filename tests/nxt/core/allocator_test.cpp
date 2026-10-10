#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <initializer_list>
#include <nxt/core/memory/allocator.hpp>

namespace {

using nxt::core::memory::SystemAllocator;

/// 주소가 요구한 정렬 경계에 맞아 떨어지는지 여부를 반환한다.
bool alignedTo(const void* pointer, std::size_t alignment) {
    return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0u;
}

} // namespace

TEST_CASE("SystemAllocator는 요청한 정렬을 만족하는 블록을 반환한다") {
    SystemAllocator allocator;

    for (const std::size_t alignment : {2u, 4u, 8u, 16u, 32u, 64u, 128u}) {
        void* const block = allocator.allocate(64u, alignment);

        REQUIRE(block != nullptr);
        CHECK(alignedTo(block, alignment));

        allocator.deallocate(block);
    }
}

TEST_CASE("SystemAllocator는 size가 0이어도 블록을 할당한다") {
    SystemAllocator allocator;

    void* const block = allocator.allocate(0u, 16u);

    // 0바이트 요청이 nullptr을 주면 Arena와 Pool이 빈 블록을 '빈 arena'로 잘못 판단한다.
    REQUIRE(block != nullptr);
    CHECK(alignedTo(block, 16u));

    allocator.deallocate(block);
}

TEST_CASE("SystemAllocator는 nullptr을 안전하게 해제한다") {
    SystemAllocator allocator;

    allocator.deallocate(nullptr);
}

TEST_CASE("SystemAllocator는 서로 다른 요청에 겹치지 않는 블록을 돌려준다") {
    SystemAllocator allocator;

    void* const first = allocator.allocate(16u, 16u);
    void* const second = allocator.allocate(16u, 16u);

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first != second);

    allocator.deallocate(first);
    allocator.deallocate(second);
}

TEST_CASE("반복 할당과 해제를 거쳐도 정렬이 유지된다") {
    SystemAllocator allocator;

    for (int i = 0; i < 1000; ++i) {
        void* const block = allocator.allocate(32u, 32u);

        REQUIRE(block != nullptr);
        CHECK(alignedTo(block, 32u));

        allocator.deallocate(block);
    }
}

TEST_CASE("SystemAllocator는 요청한 크기 전체를 사용할 수 있다") {
    SystemAllocator allocator;

    constexpr std::size_t kSize = 256u;
    auto* const block = static_cast<unsigned char*>(allocator.allocate(kSize, 16u));
    REQUIRE(block != nullptr);

    // 블록이 요청보다 짧으면 범위를 벗어난 쓰기가 일어나고, 여기서 깨진다.
    for (std::size_t i = 0; i < kSize; ++i) {
        block[i] = static_cast<unsigned char>(i);
    }
    for (std::size_t i = 0; i < kSize; ++i) {
        REQUIRE(block[i] == static_cast<unsigned char>(i));
    }

    allocator.deallocate(block);
}
