#include <compare>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <type_traits>

namespace {

using namespace nxt::graphics::rhi;

struct BufferTag {};
struct ImageTag {};
struct ImageViewTag {};

using BufferHandle = ResourceHandle<BufferTag>;
using ImageHandle = ResourceHandle<ImageTag>;
using ImageViewHandle = ResourceHandle<ImageViewTag>;

} // namespace

TEST_SUITE("graphics::rhi::OpaqueHandle") {

    TEST_CASE("64비트 부호 없는 정수 타입이다") {
        CHECK(std::is_same_v<OpaqueHandle, std::uint64_t>);
        CHECK(sizeof(OpaqueHandle) == 8);
        CHECK(std::is_unsigned_v<OpaqueHandle>);
    }
}

TEST_SUITE("graphics::rhi::ResourceHandle") {

    TEST_CASE("기본 생성 시 유효하지 않은 핸들이다") {
        constexpr BufferHandle handle{};

        CHECK_FALSE(handle.valid());
        CHECK(handle.value() == 0);
    }

    TEST_CASE("0으로 생성하면 유효하지 않은 핸들이다") {
        constexpr auto handle = BufferHandle::fromValue(0);

        CHECK_FALSE(handle.valid());
        CHECK(handle.value() == 0);
    }

    TEST_CASE("0이 아닌 값으로 생성하면 유효한 핸들이다") {
        constexpr auto handle = BufferHandle::fromValue(42);

        CHECK(handle.valid());
        CHECK(handle.value() == 42);
    }

    TEST_CASE("최대 64비트 값을 보존한다") {
        constexpr auto handle = BufferHandle::fromValue(UINT64_MAX);

        CHECK(handle.valid());
        CHECK(handle.value() == UINT64_MAX);
    }

    TEST_CASE("원시 값을 생성과 조회 과정에서 보존한다") {
        constexpr OpaqueHandle rawValue = 0x123456789ABCDEF0ULL;
        constexpr auto handle = BufferHandle::fromValue(rawValue);

        CHECK(handle.value() == rawValue);
    }

    TEST_CASE("복사 생성은 핸들 값을 보존한다") {
        constexpr auto original = BufferHandle::fromValue(123);
        constexpr BufferHandle copied{original};

        CHECK(copied.valid());
        CHECK(copied.value() == original.value());
        CHECK(copied == original);
    }

    TEST_CASE("복사 대입은 핸들 값을 보존한다") {
        constexpr auto original = BufferHandle::fromValue(456);
        BufferHandle copied{};

        copied = original;

        CHECK(copied.valid());
        CHECK(copied.value() == 456);
        CHECK(copied == original);
    }

    TEST_CASE("이동 생성은 핸들 값을 보존한다") {
        auto original = BufferHandle::fromValue(789);
        const BufferHandle moved{static_cast<BufferHandle&&>(original)};

        CHECK(moved.valid());
        CHECK(moved.value() == 789);
    }

    TEST_CASE("동일한 값을 가진 핸들은 서로 같다") {
        constexpr auto first = BufferHandle::fromValue(100);
        constexpr auto second = BufferHandle::fromValue(100);

        CHECK(first == second);
        CHECK_FALSE(first != second);
    }

    TEST_CASE("서로 다른 값을 가진 핸들은 서로 다르다") {
        constexpr auto first = BufferHandle::fromValue(100);
        constexpr auto second = BufferHandle::fromValue(200);

        CHECK(first != second);
        CHECK_FALSE(first == second);
    }

    TEST_CASE("핸들 값의 대소 관계를 비교할 수 있다") {
        constexpr auto first = BufferHandle::fromValue(100);
        constexpr auto second = BufferHandle::fromValue(200);

        CHECK(first < second);
        CHECK(first <= second);
        CHECK(second > first);
        CHECK(second >= first);
    }

    TEST_CASE("유효하지 않은 핸들도 비교할 수 있다") {
        constexpr BufferHandle invalidA{};
        constexpr auto invalidB = BufferHandle::fromValue(0);
        constexpr auto valid = BufferHandle::fromValue(1);

        CHECK(invalidA == invalidB);
        CHECK(invalidA < valid);
        CHECK(valid > invalidB);
    }

    TEST_CASE("서로 다른 태그는 서로 다른 핸들 타입이다") {
        CHECK_FALSE((std::is_same_v<BufferHandle, ImageHandle>));
        CHECK_FALSE((std::is_same_v<ImageHandle, ImageViewHandle>));
        CHECK_FALSE((std::is_same_v<BufferHandle, ImageViewHandle>));
    }

    TEST_CASE("핸들은 원시 정수로 암시적 변환되지 않는다") {
        CHECK_FALSE((std::is_convertible_v<BufferHandle, OpaqueHandle>));
        CHECK_FALSE((std::is_convertible_v<OpaqueHandle, BufferHandle>));
    }

    TEST_CASE("원시 값 생성자는 외부에서 직접 호출할 수 없다") {
        CHECK_FALSE((std::is_constructible_v<BufferHandle, OpaqueHandle>));
    }

    TEST_CASE("핸들은 기본 생성이 가능하다") {
        CHECK(std::is_default_constructible_v<BufferHandle>);
    }

    TEST_CASE("핸들은 복사와 대입이 가능하다") {
        CHECK(std::is_copy_constructible_v<BufferHandle>);
        CHECK(std::is_copy_assignable_v<BufferHandle>);
    }

    TEST_CASE("핸들은 이동과 대입이 가능하다") {
        CHECK(std::is_move_constructible_v<BufferHandle>);
        CHECK(std::is_move_assignable_v<BufferHandle>);
    }

    TEST_CASE("핸들은 예외를 발생시키지 않는 값 타입이다") {
        CHECK(std::is_nothrow_default_constructible_v<BufferHandle>);
        CHECK(std::is_nothrow_copy_constructible_v<BufferHandle>);
        CHECK(std::is_nothrow_copy_assignable_v<BufferHandle>);
        CHECK(std::is_nothrow_move_constructible_v<BufferHandle>);
        CHECK(std::is_nothrow_move_assignable_v<BufferHandle>);
    }

    TEST_CASE("핸들은 비교 연산이 가능한 타입이다") {
        CHECK(std::is_same_v<decltype(std::declval<BufferHandle>() <=> std::declval<BufferHandle>()),
                             std::strong_ordering>);
    }

    TEST_CASE("핸들의 크기는 원시 값과 동일하다") {
        CHECK(sizeof(BufferHandle) == sizeof(OpaqueHandle));
    }

    TEST_CASE("핸들은 표준 레이아웃 타입이다") {
        CHECK(std::is_standard_layout_v<BufferHandle>);
    }

    TEST_CASE("핸들은 trivially copyable 타입이다") {
        CHECK(std::is_trivially_copyable_v<BufferHandle>);
    }
}
