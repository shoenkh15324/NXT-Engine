#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/core/memory/arena.hpp>
#include <type_traits>
#include <utility>

namespace {

using nxt::core::memory::Arena;

/// 수명 관리가 필요 없는 데이터 타입이다. Arena가 받아들이는 대상이다.
struct Pod {
    int x;
    float y;
};

/// 주소가 요구한 정렬 경계에 맞아 떨어지는지 여부를 반환한다.
bool alignedTo(const void* pointer, std::size_t alignment) {
    return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0u;
}

static_assert(!std::is_copy_constructible_v<Arena>);
static_assert(!std::is_copy_assignable_v<Arena>);
static_assert(std::is_move_constructible_v<Arena>);
static_assert(std::is_move_assignable_v<Arena>);

} // namespace

TEST_CASE("새 Arena는 용량을 확보하고 아직 사용한 바이트가 없다") {
    Arena arena{256u};

    CHECK(arena.capacity() == 256u);
    CHECK(arena.bytesUsed() == 0u);
    CHECK(arena.remaining() == 256u);
    CHECK_FALSE(arena.full());
}

TEST_CASE("allocate는 정렬 경계에서 다음 블록을 시작한다") {
    Arena arena{256u};

    auto* const first = static_cast<std::byte*>(arena.allocate(10u));
    auto* const second = static_cast<std::byte*>(arena.allocate(20u));

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    // 두 번째 블록은 첫 블록을 침범하지 않고 정렬 경계에 맞아 시작한다.
    CHECK(second >= (first + 10u));
    CHECK(alignedTo(second, Arena::kDefaultAlignment));
    CHECK(arena.bytesUsed() >= 30u);
    CHECK(arena.remaining() == 256u - arena.bytesUsed());
}

TEST_CASE("남은 용량을 넘기는 할당은 실패하고 상태를 바꾸지 않는다") {
    Arena arena{64u};
    REQUIRE(arena.allocate(48u) != nullptr);

    const auto usedBefore = arena.bytesUsed();
    CHECK(arena.allocate(32u) == nullptr);

    // 실패한 할당이 offset을 흔들면 이후 모든 할당이 어긋난다.
    CHECK(arena.bytesUsed() == usedBefore);
    CHECK(arena.remaining() == 64u - usedBefore);
}

TEST_CASE("용량을 정확히 채우면 그 다음 할당은 실패한다") {
    Arena arena{64u};
    REQUIRE(arena.allocate(64u) != nullptr);

    CHECK(arena.full());
    CHECK(arena.remaining() == 0u);
    CHECK(arena.allocate(1u) == nullptr);
}

TEST_CASE("allocate는 요청한 정렬을 만족한다") {
    // 블록은 kDefaultAlignment로 할당되므로 그 이하만 요청할 수 있다.
    constexpr std::size_t kHalfAlignment = Arena::kDefaultAlignment / 2u;

    Arena arena{256u};

    auto* const half = arena.allocate(1u, kHalfAlignment);
    auto* const full = arena.allocate(1u, Arena::kDefaultAlignment);

    REQUIRE(half != nullptr);
    REQUIRE(full != nullptr);
    CHECK(alignedTo(half, kHalfAlignment));
    CHECK(alignedTo(full, Arena::kDefaultAlignment));
}

TEST_CASE("정렬 패딩이 남은 용량을 고갈시킬 수 있다") {
    constexpr std::size_t kAlignment = Arena::kDefaultAlignment;

    Arena arena{kAlignment};
    REQUIRE(arena.allocate(1u) != nullptr);

    // 1바이트만 쓴 뒤 다음 정렬 경계까지가 전부 패딩이라 요청 1바이트를 담을 수 없다.
    CHECK(arena.allocate(1u, kAlignment) == nullptr);
    CHECK(arena.bytesUsed() <= arena.capacity());
}

TEST_CASE("연속 할당 영역이 서로 덮어쓰지 않는다") {
    Arena arena{256u};

    auto* const first = arena.allocateObject<Pod>();
    auto* const second = arena.allocateObject<Pod>();

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    first->x = 1;
    first->y = 2.5f;
    second->x = 3;
    second->y = 4.5f;

    // 블록을 잘라 쓰는 방식은 앞 요청이 뒤 요청의 영역을 침범해서는 안 된다.
    CHECK(arena.bytesUsed() == 2u * sizeof(Pod));
    CHECK(first->x == 1);
    CHECK(first->y == 2.5f);
    CHECK(second->x == 3);
    CHECK(second->y == 4.5f);
}

TEST_CASE("allocateObject은 값을 저장하고 읽을 수 있는 객체를 만든다") {
    Arena arena{128u};

    auto* const value = arena.allocateObject<Pod>();
    REQUIRE(value != nullptr);

    value->x = 42;
    value->y = 1.5f;

    CHECK(value->x == 42);
    CHECK(value->y == 1.5f);
}

TEST_CASE("reset은 사용량을 되돌리되 용량은 유지한다") {
    Arena arena{128u};
    REQUIRE(arena.allocate(64u) != nullptr);

    arena.reset();

    CHECK(arena.bytesUsed() == 0u);
    CHECK(arena.remaining() == 128u);
    CHECK(arena.capacity() == 128u);
    CHECK_FALSE(arena.full());
}

TEST_CASE("reset 이후에는 같은 주소가 다시 나온다") {
    Arena arena{128u};

    void* const first = arena.allocate(32u);
    REQUIRE(first != nullptr);

    arena.reset();

    CHECK(arena.allocate(32u) == first);
}

TEST_CASE("용량이 0인 Arena는 모든 할당이 실패한다") {
    Arena arena{0u};

    CHECK(arena.capacity() == 0u);
    CHECK(arena.remaining() == 0u);
    CHECK(arena.full());
    CHECK(arena.allocate(1u) == nullptr);
    CHECK(arena.allocateObject<Pod>() == nullptr);
}

TEST_CASE("이동하면 원본은 빈 Arena가 된다") {
    Arena source{128u};
    REQUIRE(source.allocate(64u) != nullptr);

    Arena moved{std::move(source)};

    CHECK(moved.capacity() == 128u);
    CHECK(moved.bytesUsed() == 64u);
    CHECK(moved.remaining() == 64u);

    CHECK(source.capacity() == 0u);
    CHECK(source.remaining() == 0u);
    CHECK(source.allocate(1u) == nullptr);
}

TEST_CASE("이동 대입은 대상의 기존 블록을 버리고 소유권을 넘긴다") {
    Arena source{128u};
    REQUIRE(source.allocate(64u) != nullptr);

    Arena target{16u};
    REQUIRE(target.allocate(8u) != nullptr);

    target = std::move(source);

    CHECK(target.capacity() == 128u);
    CHECK(target.bytesUsed() == 64u);
    CHECK(source.capacity() == 0u);
}
