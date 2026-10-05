#include <doctest/doctest.h>
#include <nxt/core/handle/handle.hpp>
#include <nxt/core/handle/handle_manager.hpp>

namespace {

struct TestTag;

using TestHandle = nxt::core::handle::Handle<TestTag>;
using TestManager = nxt::core::handle::HandleManager<TestTag>;

} // namespace

TEST_CASE("기본 생성한 핸들은 유효하지 않다") {
    const TestHandle handle;

    CHECK_FALSE(handle.valid());
    CHECK(handle == TestHandle::invalid());
    CHECK(handle.index() == nxt::core::handle::kInvalidIndex);
}

TEST_CASE("make로 만든 핸들은 유효하고 값을 보존한다") {
    const auto handle = TestHandle::make(7u, 3u);

    CHECK(handle.valid());
    CHECK(handle.index() == 7u);
    CHECK(handle.generation() == 3u);
}

TEST_CASE("비교 연산은 defaulted 연산자로부터 파생된다") {
    const auto a = TestHandle::make(0u, 1u);
    const auto b = TestHandle::make(1u, 1u);
    const auto c = TestHandle::make(1u, 2u);

    CHECK(a != b);
    CHECK(a < b);
    CHECK(b < c);
    CHECK_FALSE(a == b);
}

TEST_CASE("index가 무효값이면 세대가 있어도 유효하지 않다") {
    CHECK_FALSE(TestHandle::make(nxt::core::handle::kInvalidIndex, 1u).valid());
}

TEST_CASE("manager가 만든 핸들은 유효하다") {
    TestManager manager;
    const auto handle = manager.create();

    CHECK(handle.valid());
    CHECK(manager.contains(handle));
}

TEST_CASE("여러 핸들은 서로 다른 슬롯을 사용한다") {
    TestManager manager;

    const auto a = manager.create();
    const auto b = manager.create();
    const auto c = manager.create();

    CHECK(a.index() != b.index());
    CHECK(b.index() != c.index());
    CHECK(manager.contains(a));
    CHECK(manager.contains(b));
    CHECK(manager.contains(c));
}

TEST_CASE("destroy한 핸들은 stale가 되고 재호출은 실패한다") {
    TestManager manager;
    const auto handle = manager.create();

    REQUIRE(manager.destroy(handle));
    CHECK_FALSE(manager.contains(handle));
    CHECK_FALSE(manager.destroy(handle));
}

TEST_CASE("해제된 슬롯은 세대가 증가한 핸들로 재사용된다") {
    TestManager manager;
    const auto first = manager.create();
    REQUIRE(manager.destroy(first));

    const auto second = manager.create();

    CHECK(second.index() == first.index());
    CHECK(second.generation() == first.generation() + 1u);
    CHECK(manager.contains(second));
    CHECK_FALSE(manager.contains(first));
}

TEST_CASE("세대를 반복해서 증가시킬 수 있다") {
    TestManager manager;
    auto handle = manager.create();
    const auto firstGeneration = handle.generation();

    for (int i = 0; i < 8; ++i) {
        REQUIRE(manager.destroy(handle));
        handle = manager.create();
    }

    CHECK(handle.generation() == firstGeneration + 8u);
    CHECK(manager.contains(handle));
}

TEST_CASE("범위 밖 핸들은 거부한다") {
    TestManager manager;
    const auto handle = manager.create();
    CHECK(manager.contains(handle));

    CHECK_FALSE(manager.contains(TestHandle::make(999u, 1u)));
    CHECK_FALSE(manager.contains(TestHandle::invalid()));
}

// 죽은 슬롯의 현재 세대를 아는 핸들은 살아 있지 않으므로 거부되어야 한다.
// 세대 비교만으로는 이 경우를 걸러낼 수 없다.
TEST_CASE("생존하지 않는 슬롯을 가리키는 핸들은 거부한다") {
    TestManager manager;
    const auto handle = manager.create();
    REQUIRE(manager.destroy(handle));

    const auto forged = TestHandle::make(handle.index(), handle.generation() + 1u);
    CHECK_FALSE(manager.contains(forged));

    const auto reused = manager.create();
    CHECK(reused.valid());
    CHECK(manager.contains(forged));
    CHECK(reused == forged);
}

TEST_CASE("한 핸들을 해제해도 다른 핸들은 유효하다") {
    TestManager manager;
    const auto a = manager.create();
    const auto b = manager.create();
    const auto c = manager.create();

    REQUIRE(manager.destroy(b));

    CHECK(manager.contains(a));
    CHECK_FALSE(manager.contains(b));
    CHECK(manager.contains(c));
}

// 핸들 값은 핸들을 발급한 manager 안에서만 의미를 갖는다.
TEST_CASE("manager는 서로 독립적이다") {
    TestManager first;
    TestManager second;

    const auto handle = first.create();

    CHECK(first.contains(handle));
    CHECK_FALSE(second.contains(handle));
}
