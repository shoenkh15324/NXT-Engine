#include <array>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/core/memory/pool.hpp>
#include <type_traits>

namespace {

using nxt::core::memory::Pool;

int gConstructCount = 0;
int gDestructCount = 0;

/// 수명 관리가 필요한 타입이다. Pool이 생성자와 소멸자를 호출해야 한다.
struct Tracked {
    int value;

    Tracked() : value(0) {
        ++gConstructCount;
    }

    explicit Tracked(int initial) : value(initial) {
        ++gConstructCount;
    }

    ~Tracked() {
        ++gDestructCount;
    }
};

/// 기본 생성자가 없는 타입이다. allocateConstruct로만 만들 수 있다.
struct NoDefault {
    explicit NoDefault(int initial) : value(initial) {}

    int value;
};

/// 수명 관리가 필요 없는 타입이다.
struct Pod {
    int x;
};

/// 과도 정렬을 요구하는 타입이다. Pool이 지원하지 않는 경계에 있는 타입이다.
struct alignas(64) OverAligned {
    std::uint64_t value;
};

/// 주소가 요구한 정렬 경계에 맞아 떨어지는지 여부를 반환한다.
bool alignedTo(const void* pointer, std::size_t alignment) {
    return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0u;
}

static_assert(!std::is_copy_constructible_v<Pool<Pod>>);
static_assert(!std::is_copy_assignable_v<Pool<Pod>>);
static_assert(!std::is_move_constructible_v<Pool<Pod>>);
static_assert(!std::is_move_assignable_v<Pool<Pod>>);

// Pool은 과도 정렬 타입을 컴파일 타임에 거절한다. 지원 여부는 런타임에 드러나지 않으므로
// 여기서 경계를 고정해 둔다.
static_assert(alignof(OverAligned) > alignof(std::max_align_t));
static_assert(alignof(Pod) <= alignof(std::max_align_t));

} // namespace

TEST_CASE("새 Pool은 모든 슬롯을 비워 둔 상태로 시작한다") {
    Pool<Tracked> pool{8u};

    CHECK(pool.capacity() == 8u);
    CHECK(pool.available() == 8u);
    CHECK(pool.liveCount() == 0u);
}

TEST_CASE("allocate는 용량만큼 성공하고 그 다음부터 실패한다") {
    Pool<Tracked> pool{4u};

    std::array<Tracked*, 4> objects{};
    for (auto& object : objects) {
        object = pool.allocate();
        REQUIRE(object != nullptr);
    }

    CHECK(pool.liveCount() == 4u);
    CHECK(pool.available() == 0u);
    CHECK(pool.allocate() == nullptr);

    for (auto* const object : objects) {
        pool.deallocate(object);
    }
    CHECK(pool.liveCount() == 0u);
}

TEST_CASE("고갈된 allocate는 liveCount를 증가시키지 않는다") {
    Pool<Tracked> pool{2u};

    std::array<Tracked*, 2> objects{};
    for (auto& object : objects) {
        object = pool.allocate();
        REQUIRE(object != nullptr);
    }

    CHECK(pool.allocate() == nullptr);

    // 실패한 할당까지 카운터를 세면 실제보다 liveCount가 커져 소멸자 assertion이
    // 사용자가 잘못하지 않았는데도 나중에 발화한다.
    CHECK(pool.liveCount() == 2u);

    for (auto* const object : objects) {
        pool.deallocate(object);
    }
    CHECK(pool.liveCount() == 0u);
}

TEST_CASE("deallocate는 슬롯을 반환해 다시 사용할 수 있게 한다") {
    Pool<Tracked> pool{2u};

    auto* const first = pool.allocate();
    REQUIRE(first != nullptr);

    pool.deallocate(first);
    CHECK(pool.available() == 2u);
    CHECK(pool.liveCount() == 0u);

    auto* const second = pool.allocate();
    REQUIRE(second != nullptr);
    CHECK(second == first);

    pool.deallocate(second);
}

TEST_CASE("슬롯은 앞에서부터 순서대로 사용된다") {
    Pool<Tracked> pool{4u};

    std::array<Tracked*, 4> objects{};
    for (auto& object : objects) {
        object = pool.allocate();
        REQUIRE(object != nullptr);
    }

    for (std::size_t i = 1; i < objects.size(); ++i) {
        // free list를 뒤에서부터 채워야 인접한 슬롯이 순서대로 나오고 연속 접근이 유지된다.
        CHECK(objects[i] == (objects[i - 1] + 1));
    }

    for (auto* const object : objects) {
        pool.deallocate(object);
    }
}

TEST_CASE("allocateConstruct는 생성자에 인자를 전달한다") {
    Pool<Tracked> pool{2u};

    auto* const value = pool.allocateConstruct(7);
    REQUIRE(value != nullptr);
    CHECK(value->value == 7);

    pool.deallocate(value);
}

TEST_CASE("기본 생성자가 없는 타입도 만들 수 있다") {
    Pool<NoDefault> pool{2u};

    auto* const value = pool.allocateConstruct(9);
    REQUIRE(value != nullptr);
    CHECK(value->value == 9);

    pool.deallocate(value);
}

TEST_CASE("재사용한 슬롯은 값 초기화되어 이전 데이터가 남지 않는다") {
    Pool<Pod> pool{1u};

    auto* const first = pool.allocate();
    REQUIRE(first != nullptr);
    first->x = 1234;

    pool.deallocate(first);

    auto* const second = pool.allocate();
    REQUIRE(second == first);

    // 값 초기화가 아니면 앞선 객체의 값이 새 객체에 그대로 새어 나온다.
    CHECK(second->x == 0);

    pool.deallocate(second);
}

TEST_CASE("생성자와 소멸자가 실제 객체 수명만큼 호출된다") {
    gConstructCount = 0;
    gDestructCount = 0;

    {
        Pool<Tracked> pool{2u};

        auto* const first = pool.allocate();
        REQUIRE(first != nullptr);

        auto* const second = pool.allocateConstruct(1);
        REQUIRE(second != nullptr);

        CHECK(gConstructCount == 2);
        CHECK(gDestructCount == 0);

        pool.deallocate(first);
        CHECK(gDestructCount == 1);

        pool.deallocate(second);
        CHECK(gDestructCount == 2);
    }

    CHECK(gConstructCount == 2);
    CHECK(gDestructCount == 2);
}

TEST_CASE("블록 정렬이 타입의 정렬 요구를 만족한다") {
    Pool<Pod> pool{4u};

    auto* const object = pool.allocate();
    REQUIRE(object != nullptr);

    CHECK(alignedTo(object, alignof(Pod)));

    pool.deallocate(object);
}

TEST_CASE("owns는 이 풀의 슬롯 주소를 참으로 판정한다") {
    Pool<Pod> pool{4u};

    auto* const object = pool.allocate();
    REQUIRE(object != nullptr);
    CHECK(pool.owns(object));

    pool.deallocate(object);
}

TEST_CASE("owns는 풀 밖의 주소를 거짓으로 판정한다") {
    Pool<Pod> pool{4u};

    int local = 0;
    CHECK_FALSE(pool.owns(&local));
    CHECK_FALSE(pool.owns(nullptr));
}

TEST_CASE("슬롯이 0개인 Pool은 모든 할당이 실패한다") {
    Pool<Pod> pool{0u};

    CHECK(pool.capacity() == 0u);
    CHECK(pool.available() == 0u);
    CHECK(pool.allocate() == nullptr);

    int local = 0;
    CHECK_FALSE(pool.owns(&local));
}

TEST_CASE("Pool은 서로 독립적이다") {
    Pool<Pod> first{2u};
    Pool<Pod> second{2u};

    auto* const object = first.allocate();
    REQUIRE(object != nullptr);

    CHECK(first.owns(object));
    CHECK_FALSE(second.owns(object));

    // 한쪽이 고갈되어도 다른 쪽은 영향을 받지 않는다.
    auto* const other = second.allocate();
    REQUIRE(other != nullptr);

    first.deallocate(object);
    second.deallocate(other);
}
