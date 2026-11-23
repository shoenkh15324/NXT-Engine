
#include <cmath>
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/math/mat4.hpp>

using nxt::math::dot;
using nxt::math::length;
using nxt::math::lookAt;
using nxt::math::Mat4;
using nxt::math::perspective;
using nxt::math::rotationX;
using nxt::math::rotationY;
using nxt::math::rotationZ;
using nxt::math::scale;
using nxt::math::transformDirection;
using nxt::math::transformPoint;
using nxt::math::translation;
using nxt::math::Vec3;

namespace {

constexpr float kEpsilon = 1e-5f;
constexpr float kPi = 3.14159265358979323846f;

void checkVec3(const Vec3& actual, const Vec3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(kEpsilon));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(kEpsilon));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(kEpsilon));
}

void checkMat4(const Mat4& actual, const Mat4& expected) {
    for (std::uint32_t row = 0; row < 4u; ++row) {
        for (std::uint32_t column = 0; column < 4u; ++column) {
            CHECK(actual(row, column) == doctest::Approx(expected(row, column)).epsilon(kEpsilon));
        }
    }
}

} // namespace

TEST_SUITE("math::Mat4") {

    // ── 생성 및 접근 ──────────────────────────────────────

    TEST_CASE("기본 생성 시 단위 행렬을 반환한다") {
        constexpr Mat4 value{};

        for (std::uint32_t row = 0; row < 4u; ++row) {
            for (std::uint32_t column = 0; column < 4u; ++column) {
                const float expected = (row == column) ? 1.0f : 0.0f;
                CHECK(value(row, column) == expected);
            }
        }
    }

    TEST_CASE("identity 함수는 단위 행렬을 반환한다") {
        checkMat4(Mat4::identity(), Mat4{});
    }

    TEST_CASE("성분에 행과 열을 지정해 접근한다") {
        Mat4 value{};

        value(0, 1) = 2.0f;
        value(2, 3) = 5.0f;

        CHECK(value(0, 1) == 2.0f);
        CHECK(value(2, 3) == 5.0f);
        CHECK(value(1, 0) == 0.0f);
        CHECK(value(3, 2) == 0.0f);
    }

    TEST_CASE("const 행렬에서도 성분을 읽을 수 있다") {
        const Mat4 value = translation(Vec3{1.0f, 2.0f, 3.0f});

        CHECK(value(0, 3) == 1.0f);
        CHECK(value(1, 3) == 2.0f);
        CHECK(value(2, 3) == 3.0f);
    }

    TEST_CASE("data 함수는 열 우선으로 저장된 성분을 반환한다") {
        Mat4 value{};

        value(0, 1) = 2.0f;
        value(2, 3) = 5.0f;

        const float* data = value.data();

        CHECK(data[0] == 1.0f);
        CHECK(data[4] == 2.0f);
        CHECK(data[14] == 5.0f);
        CHECK(data[15] == 1.0f);
    }

    TEST_CASE("data 함수는 내부 성분 배열의 시작 주소를 반환한다") {
        const Mat4 value{};

        CHECK(value.data() != nullptr);
        CHECK(value.data()[0] == 1.0f);
        CHECK(value.data()[15] == 1.0f);
    }

    // ── 행렬 곱셈 ─────────────────────────────────────────

    TEST_CASE("단위 행렬과의 곱셈은 원래 행렬을 유지한다") {
        const Mat4 value = translation(Vec3{1.0f, 2.0f, 3.0f}) * scale(Vec3{2.0f, 3.0f, 4.0f});

        checkMat4(Mat4::identity() * value, value);
        checkMat4(value * Mat4::identity(), value);
    }

    TEST_CASE("행렬 곱셈으로 두 변환을 결합한다") {
        const Mat4 lhs = translation(Vec3{10.0f, 20.0f, 30.0f});
        const Mat4 rhs = scale(Vec3{2.0f, 3.0f, 4.0f});

        const Mat4 result = lhs * rhs;

        checkVec3(transformPoint(result, Vec3{1.0f, 1.0f, 1.0f}), Vec3{12.0f, 23.0f, 34.0f});
    }

    TEST_CASE("행렬 곱셈은 오른쪽 행렬의 변환을 먼저 적용한다") {
        const Mat4 move = translation(Vec3{10.0f, 0.0f, 0.0f});
        const Mat4 resize = scale(2.0f);

        const Vec3 point{1.0f, 0.0f, 0.0f};

        const Vec3 result = transformPoint(move * resize, point);

        checkVec3(result, Vec3{12.0f, 0.0f, 0.0f});
    }

    TEST_CASE("행렬 곱셈은 일반적으로 교환 법칙을 만족하지 않는다") {
        const Mat4 move = translation(Vec3{1.0f, 0.0f, 0.0f});
        const Mat4 resize = scale(2.0f);

        const Vec3 point{1.0f, 0.0f, 0.0f};

        const Vec3 first = transformPoint(move * resize, point);
        const Vec3 second = transformPoint(resize * move, point);

        checkVec3(first, Vec3{3.0f, 0.0f, 0.0f});
        checkVec3(second, Vec3{4.0f, 0.0f, 0.0f});
    }

    // ── 좌표 변환 ─────────────────────────────────────────

    TEST_CASE("단위 행렬로 점을 변환하면 원래 좌표를 유지한다") {
        const Vec3 point{1.0f, 2.0f, 3.0f};

        checkVec3(transformPoint(Mat4::identity(), point), point);
    }

    TEST_CASE("이동 행렬로 점의 위치를 변경한다") {
        const Mat4 matrix = translation(Vec3{10.0f, 20.0f, 30.0f});

        const Vec3 result = transformPoint(matrix, Vec3{1.0f, 2.0f, 3.0f});

        checkVec3(result, Vec3{11.0f, 22.0f, 33.0f});
    }

    TEST_CASE("크기 행렬로 점의 각 축 배율을 변경한다") {
        const Mat4 matrix = scale(Vec3{2.0f, 3.0f, 4.0f});

        const Vec3 result = transformPoint(matrix, Vec3{1.0f, 2.0f, 3.0f});

        checkVec3(result, Vec3{2.0f, 6.0f, 12.0f});
    }

    TEST_CASE("방향 벡터 변환에는 이동이 적용되지 않는다") {
        const Mat4 matrix = translation(Vec3{10.0f, 20.0f, 30.0f});

        const Vec3 result = transformDirection(matrix, Vec3{1.0f, 2.0f, 3.0f});

        checkVec3(result, Vec3{1.0f, 2.0f, 3.0f});
    }

    TEST_CASE("방향 벡터 변환에는 크기 배율이 적용된다") {
        const Mat4 matrix = scale(Vec3{2.0f, 3.0f, 4.0f});

        const Vec3 result = transformDirection(matrix, Vec3{1.0f, 2.0f, 3.0f});

        checkVec3(result, Vec3{2.0f, 6.0f, 12.0f});
    }

    // ── 이동 및 크기 변환 ─────────────────────────────────

    TEST_CASE("이동 행렬은 지정한 이동량을 저장한다") {
        const Mat4 result = translation(Vec3{1.0f, 2.0f, 3.0f});

        CHECK(result(0, 3) == 1.0f);
        CHECK(result(1, 3) == 2.0f);
        CHECK(result(2, 3) == 3.0f);

        CHECK(result(0, 0) == 1.0f);
        CHECK(result(1, 1) == 1.0f);
        CHECK(result(2, 2) == 1.0f);
    }

    TEST_CASE("축별 크기 행렬은 지정한 배율을 저장한다") {
        const Mat4 result = scale(Vec3{2.0f, 3.0f, 4.0f});

        CHECK(result(0, 0) == 2.0f);
        CHECK(result(1, 1) == 3.0f);
        CHECK(result(2, 2) == 4.0f);
        CHECK(result(3, 3) == 1.0f);
    }

    TEST_CASE("균일 크기 행렬은 모든 축에 같은 배율을 적용한다") {
        const Mat4 result = scale(3.0f);

        CHECK(result(0, 0) == 3.0f);
        CHECK(result(1, 1) == 3.0f);
        CHECK(result(2, 2) == 3.0f);
        CHECK(result(3, 3) == 1.0f);
    }

    // ── 회전 변환 ─────────────────────────────────────────

    TEST_CASE("X축 기준 90도 회전으로 Y축을 Z축 방향으로 변환한다") {
        const Mat4 matrix = rotationX(kPi * 0.5f);

        const Vec3 result = transformDirection(matrix, Vec3{0.0f, 1.0f, 0.0f});

        checkVec3(result, Vec3{0.0f, 0.0f, 1.0f});
    }

    TEST_CASE("Y축 기준 90도 회전으로 Z축을 X축 방향으로 변환한다") {
        const Mat4 matrix = rotationY(kPi * 0.5f);

        const Vec3 result = transformDirection(matrix, Vec3{0.0f, 0.0f, 1.0f});

        checkVec3(result, Vec3{1.0f, 0.0f, 0.0f});
    }

    TEST_CASE("Z축 기준 90도 회전으로 X축을 Y축 방향으로 변환한다") {
        const Mat4 matrix = rotationZ(kPi * 0.5f);

        const Vec3 result = transformDirection(matrix, Vec3{1.0f, 0.0f, 0.0f});

        checkVec3(result, Vec3{0.0f, 1.0f, 0.0f});
    }

    TEST_CASE("X축 기준 0도 회전은 벡터를 유지한다") {
        const Mat4 matrix = rotationX(0.0f);
        const Vec3 point{1.0f, 2.0f, 3.0f};

        checkVec3(transformDirection(matrix, point), point);
    }

    TEST_CASE("Y축 기준 0도 회전은 벡터를 유지한다") {
        const Mat4 matrix = rotationY(0.0f);
        const Vec3 point{1.0f, 2.0f, 3.0f};

        checkVec3(transformDirection(matrix, point), point);
    }

    TEST_CASE("Z축 기준 0도 회전은 벡터를 유지한다") {
        const Mat4 matrix = rotationZ(0.0f);
        const Vec3 point{1.0f, 2.0f, 3.0f};

        checkVec3(transformDirection(matrix, point), point);
    }

    // ── 원근 투영 ─────────────────────────────────────────

    TEST_CASE("원근 투영 행렬은 Vulkan 깊이 범위의 가까운 평면을 0으로 매핑한다") {
        const Mat4 matrix = perspective(kPi * 0.5f, 1.0f, 1.0f, 10.0f);

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 0.0f, -1.0f});

        CHECK(result.z == doctest::Approx(0.0f).epsilon(kEpsilon));
    }

    TEST_CASE("원근 투영 행렬은 Vulkan 깊이 범위의 먼 평면을 1로 매핑한다") {
        const Mat4 matrix = perspective(kPi * 0.5f, 1.0f, 1.0f, 10.0f);

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 0.0f, -10.0f});

        CHECK(result.z == doctest::Approx(1.0f).epsilon(kEpsilon));
    }

    TEST_CASE("원근 투영 행렬은 시야각과 종횡비에 따라 X 좌표를 투영한다") {
        const Mat4 matrix = perspective(kPi * 0.5f, 2.0f, 1.0f, 10.0f);

        const Vec3 result = transformPoint(matrix, Vec3{1.0f, 0.0f, -1.0f});

        CHECK(result.x == doctest::Approx(0.5f).epsilon(kEpsilon));
    }

    TEST_CASE("원근 투영 행렬은 Vulkan 좌표계에 맞게 Y축을 반전한다") {
        const Mat4 matrix = perspective(kPi * 0.5f, 1.0f, 1.0f, 10.0f);

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 1.0f, -1.0f});

        CHECK(result.y == doctest::Approx(-1.0f).epsilon(kEpsilon));
    }

    TEST_CASE("원근 투영 행렬은 카메라 앞쪽의 점을 양의 W로 변환한다") {
        const Mat4 matrix = perspective(kPi * 0.5f, 1.0f, 1.0f, 10.0f);

        CHECK(matrix(3, 2) == -1.0f);
        CHECK(matrix(3, 3) == 0.0f);
    }

    // ── 뷰 행렬 ───────────────────────────────────────────

    TEST_CASE("카메라 위치는 뷰 공간 원점으로 변환된다") {
        const Vec3 eye{0.0f, 0.0f, 5.0f};
        const Mat4 matrix = lookAt(eye, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        checkVec3(transformPoint(matrix, eye), Vec3{});
    }

    TEST_CASE("카메라가 원점을 바라보면 원점은 카메라 앞쪽에 위치한다") {
        const Mat4 matrix = lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 0.0f, 0.0f});

        checkVec3(result, Vec3{0.0f, 0.0f, -5.0f});
    }

    TEST_CASE("카메라의 오른쪽에 있는 점은 양의 X 좌표를 갖는다") {
        const Mat4 matrix = lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        const Vec3 result = transformPoint(matrix, Vec3{1.0f, 0.0f, 0.0f});

        CHECK(result.x == doctest::Approx(1.0f));
    }

    TEST_CASE("카메라의 위쪽에 있는 점은 양의 Y 좌표를 갖는다") {
        const Mat4 matrix = lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 1.0f, 0.0f});

        CHECK(result.y == doctest::Approx(1.0f));
    }

    TEST_CASE("카메라의 시선 방향에 있는 점은 음의 Z 좌표를 갖는다") {
        const Mat4 matrix = lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        const Vec3 result = transformPoint(matrix, Vec3{0.0f, 0.0f, 0.0f});

        CHECK(result.z < 0.0f);
    }

    TEST_CASE("이동한 카메라도 올바른 뷰 공간 좌표를 생성한다") {
        const Mat4 matrix = lookAt(Vec3{3.0f, 2.0f, 5.0f}, Vec3{3.0f, 2.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

        const Vec3 result = transformPoint(matrix, Vec3{3.0f, 2.0f, 0.0f});

        checkVec3(result, Vec3{0.0f, 0.0f, -5.0f});
    }
}
