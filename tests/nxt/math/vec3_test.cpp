
#include <doctest/doctest.h>
#include <nxt/math/vec3.hpp>

using nxt::math::cross;
using nxt::math::dot;
using nxt::math::length;
using nxt::math::lengthSquared;
using nxt::math::normalized;
using nxt::math::Vec3;

namespace {

constexpr float kEpsilon = 1e-5f;

void checkVec3(const Vec3& actual, const Vec3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(kEpsilon));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(kEpsilon));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(kEpsilon));
}

} // namespace

TEST_SUITE("math::Vec3") {

    // ── 생성 ──────────────────────────────────────────────

    TEST_CASE("기본 생성 시 영 벡터를 반환한다") {
        constexpr Vec3 value{};

        CHECK(value.x == 0.0f);
        CHECK(value.y == 0.0f);
        CHECK(value.z == 0.0f);
    }

    TEST_CASE("스칼라 생성 시 모든 성분을 같은 값으로 초기화한다") {
        constexpr Vec3 value{5.0f};

        CHECK(value.x == 5.0f);
        CHECK(value.y == 5.0f);
        CHECK(value.z == 5.0f);
    }

    TEST_CASE("성분별 생성 시 지정한 값으로 초기화한다") {
        constexpr Vec3 value{1.0f, 2.0f, 3.0f};

        CHECK(value.x == 1.0f);
        CHECK(value.y == 2.0f);
        CHECK(value.z == 3.0f);
    }

    // ── 산술 연산 ─────────────────────────────────────────

    TEST_CASE("복합 덧셈 연산으로 각 성분을 더한다") {
        Vec3 value{1.0f, 2.0f, 3.0f};
        value += Vec3{4.0f, 5.0f, 6.0f};

        checkVec3(value, Vec3{5.0f, 7.0f, 9.0f});
    }

    TEST_CASE("복합 뺄셈 연산으로 각 성분을 뺀다") {
        Vec3 value{5.0f, 7.0f, 9.0f};
        value -= Vec3{1.0f, 2.0f, 3.0f};

        checkVec3(value, Vec3{4.0f, 5.0f, 6.0f});
    }

    TEST_CASE("복합 곱셈 연산으로 모든 성분에 스칼라를 곱한다") {
        Vec3 value{1.0f, 2.0f, 3.0f};
        value *= 2.0f;

        checkVec3(value, Vec3{2.0f, 4.0f, 6.0f});
    }

    TEST_CASE("복합 나눗셈 연산으로 모든 성분을 스칼라로 나눈다") {
        Vec3 value{2.0f, 4.0f, 6.0f};
        value /= 2.0f;

        checkVec3(value, Vec3{1.0f, 2.0f, 3.0f});
    }

    TEST_CASE("덧셈 연산으로 두 벡터의 각 성분을 더한다") {
        const Vec3 result = Vec3{1.0f, 2.0f, 3.0f} + Vec3{4.0f, 5.0f, 6.0f};

        checkVec3(result, Vec3{5.0f, 7.0f, 9.0f});
    }

    TEST_CASE("뺄셈 연산으로 두 벡터의 각 성분을 뺀다") {
        const Vec3 result = Vec3{5.0f, 7.0f, 9.0f} - Vec3{1.0f, 2.0f, 3.0f};

        checkVec3(result, Vec3{4.0f, 5.0f, 6.0f});
    }

    TEST_CASE("단항 마이너스 연산으로 벡터의 방향을 반전한다") {
        const Vec3 result = -Vec3{1.0f, -2.0f, 3.0f};

        checkVec3(result, Vec3{-1.0f, 2.0f, -3.0f});
    }

    TEST_CASE("벡터와 스칼라의 곱셈을 수행한다") {
        const Vec3 result = Vec3{1.0f, 2.0f, 3.0f} * 2.0f;

        checkVec3(result, Vec3{2.0f, 4.0f, 6.0f});
    }

    TEST_CASE("스칼라와 벡터의 곱셈을 수행한다") {
        const Vec3 result = 2.0f * Vec3{1.0f, 2.0f, 3.0f};

        checkVec3(result, Vec3{2.0f, 4.0f, 6.0f});
    }

    TEST_CASE("벡터의 스칼라 나눗셈을 수행한다") {
        const Vec3 result = Vec3{2.0f, 4.0f, 6.0f} / 2.0f;

        checkVec3(result, Vec3{1.0f, 2.0f, 3.0f});
    }

    // ── 벡터 연산 ─────────────────────────────────────────

    TEST_CASE("내적으로 두 벡터의 스칼라곱을 계산한다") {
        constexpr float result = dot(Vec3{1.0f, 2.0f, 3.0f}, Vec3{4.0f, 5.0f, 6.0f});

        CHECK(result == doctest::Approx(32.0f));
    }

    TEST_CASE("서로 수직인 두 벡터의 내적은 0이다") {
        constexpr float result = dot(Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        CHECK(result == doctest::Approx(0.0f));
    }

    TEST_CASE("외적으로 오른손 좌표계 기준 벡터를 계산한다") {
        const Vec3 result = cross(Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        checkVec3(result, Vec3{0.0f, 0.0f, 1.0f});
    }

    TEST_CASE("외적은 피연산자의 순서를 바꾸면 방향이 반전된다") {
        const Vec3 lhs{1.0f, 0.0f, 0.0f};
        const Vec3 rhs{0.0f, 1.0f, 0.0f};

        checkVec3(cross(rhs, lhs), -cross(lhs, rhs));
    }

    TEST_CASE("외적 결과는 두 피연산자 모두와 수직이다") {
        const Vec3 lhs{1.0f, 2.0f, 3.0f};
        const Vec3 rhs{4.0f, 5.0f, 6.0f};
        const Vec3 result = cross(lhs, rhs);

        CHECK(dot(result, lhs) == doctest::Approx(0.0f).epsilon(kEpsilon));
        CHECK(dot(result, rhs) == doctest::Approx(0.0f).epsilon(kEpsilon));
    }

    // ── 길이 계산 ─────────────────────────────────────────

    TEST_CASE("길이 제곱으로 벡터 크기의 제곱을 계산한다") {
        constexpr float result = lengthSquared(Vec3{2.0f, 3.0f, 6.0f});

        CHECK(result == doctest::Approx(49.0f));
    }

    TEST_CASE("벡터의 길이를 계산한다") {
        const float result = length(Vec3{2.0f, 3.0f, 6.0f});

        CHECK(result == doctest::Approx(7.0f));
    }

    TEST_CASE("영 벡터의 길이는 0이다") {
        const float result = length(Vec3{});

        CHECK(result == doctest::Approx(0.0f));
    }

    // ── 정규화 ─────────────────────────────────────────────

    TEST_CASE("정규화 결과의 길이는 1이다") {
        const Vec3 result = normalized(Vec3{3.0f, 0.0f, 4.0f});

        checkVec3(result, Vec3{0.6f, 0.0f, 0.8f});
        CHECK(length(result) == doctest::Approx(1.0f));
    }

    TEST_CASE("정규화 후에도 원래 벡터의 방향을 유지한다") {
        const Vec3 original{2.0f, 3.0f, 6.0f};
        const Vec3 result = normalized(original);

        CHECK(dot(original, result) > 0.0f);
        CHECK(length(result) == doctest::Approx(1.0f));
    }

    TEST_CASE("이미 정규화된 벡터를 다시 정규화해도 값이 유지된다") {
        const Vec3 original{1.0f, 0.0f, 0.0f};
        const Vec3 result = normalized(original);

        checkVec3(result, original);
    }
}
