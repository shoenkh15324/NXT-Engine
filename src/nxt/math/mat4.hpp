#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <nxt/math/vec3.hpp>

namespace nxt::math {

/**
 * @brief 4x4 행렬이다.
 *
 * 열 우선(column-major)으로 저장한다. elements_[column * 4 + row] 배치이며
 * GLSL의 mat4와 같다. data()가 가리키는 16개 float를 uniform buffer에 그대로
 * 올릴 수 있다.
 *
 * 기본 생성 결과는 단위 행렬이다. 변환을 누적할 때 초기값으로 바로 쓸 수 있다.
 */
class Mat4 {
public:
    /// 단위 행렬을 만든다.
    constexpr Mat4() noexcept
        : elements_{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f} {}

    /// 단위 행렬을 반환한다.
    [[nodiscard]]
    static constexpr Mat4 identity() noexcept {
        return Mat4{};
    }

    /**
     * @brief 행과 열을 지정해 성분에 접근한다.
     *
     * 수학 표기와 같은 (row, column) 순서다. 내부 저장이 열 우선이므로
     * 실제 인덱스는 column * 4 + row이다.
     *
     * @param[in] row     행. 0 이상 3 이하.
     * @param[in] column  열. 0 이상 3 이하.
     * @return 해당 성분의 참조.
     */
    [[nodiscard]]
    constexpr float& operator()(std::uint32_t row, std::uint32_t column) noexcept {
        return elements_[(column * 4u) + row];
    }

    /// @brief 행과 열을 지정해 성분을 읽는다. 인덱스 조건은 비const 버전과 같다.
    [[nodiscard]]
    constexpr const float& operator()(std::uint32_t row, std::uint32_t column) const noexcept {
        return elements_[(column * 4u) + row];
    }

    /// 열 우선으로 저장된 16개 성분의 시작 주소를 반환한다.
    [[nodiscard]]
    constexpr const float* data() const noexcept {
        return elements_.data();
    }

private:
    /// 열 우선으로 저장한 16개의 성분.
    std::array<float, 16> elements_;
};

/**
 * @brief 두 행렬을 곱한다.
 *
 * column-vector 규약이다. a * b는 b를 먼저 적용하고 a를 나중에 적용한다.
 */
[[nodiscard]]
constexpr Mat4 operator*(const Mat4& lhs, const Mat4& rhs) noexcept {
    Mat4 result;
    for (std::uint32_t column = 0; column < 4u; ++column) {
        for (std::uint32_t row = 0; row < 4u; ++row) {
            float value = 0.0f;
            for (std::uint32_t k = 0; k < 4u; ++k) {
                value += lhs(row, k) * rhs(k, column);
            }
            result(row, column) = value;
        }
    }
    return result;
}

/**
 * @brief 점을 변환한다.
 *
 * 점을 (x, y, z, 1)로 확장해 곱한 뒤 w로 나눈다. 아핀 변환에서는 w가 1이라
 * 나눗셈이 항등이고, 투영 행렬에서는 원근 나눗셈이 된다.
 *
 * @param[in] matrix 변환 행렬.
 * @param[in] point  변환할 점.
 * @return 변환된 점. w가 0이면 결과는 정의되지 않는다.
 */
[[nodiscard]]
constexpr Vec3 transformPoint(const Mat4& matrix, const Vec3& point) noexcept {
    const float x = (matrix(0, 0) * point.x) + (matrix(0, 1) * point.y) + (matrix(0, 2) * point.z) + matrix(0, 3);
    const float y = (matrix(1, 0) * point.x) + (matrix(1, 1) * point.y) + (matrix(1, 2) * point.z) + matrix(1, 3);
    const float z = (matrix(2, 0) * point.x) + (matrix(2, 1) * point.y) + (matrix(2, 2) * point.z) + matrix(2, 3);
    const float w = (matrix(3, 0) * point.x) + (matrix(3, 1) * point.y) + (matrix(3, 2) * point.z) + matrix(3, 3);
    return {x / w, y / w, z / w};
}

/**
 * @brief 방향 벡터를 변환한다.
 *
 * 이동을 적용하지 않고 선형 부분만 곱한다. w로 나누지도 않는다.
 *
 * @param[in] matrix 변환 행렬.
 * @param[in] direction 변환할 방향.
 * @return 변환된 방향.
 */
[[nodiscard]]
constexpr Vec3 transformDirection(const Mat4& matrix, const Vec3& direction) noexcept {
    return {(matrix(0, 0) * direction.x) + (matrix(0, 1) * direction.y) + (matrix(0, 2) * direction.z),
            (matrix(1, 0) * direction.x) + (matrix(1, 1) * direction.y) + (matrix(1, 2) * direction.z),
            (matrix(2, 0) * direction.x) + (matrix(2, 1) * direction.y) + (matrix(2, 2) * direction.z)};
}

/**
 * @brief 이동 행렬을 만든다.
 * @param[in] offset 각 축의 이동량.
 */
[[nodiscard]]
constexpr Mat4 translation(const Vec3 offset) noexcept {
    Mat4 result;
    result(0, 3) = offset.x;
    result(1, 3) = offset.y;
    result(2, 3) = offset.z;
    return result;
}

/**
 * @brief 축별 크기 행렬을 만든다.
 * @param[in] factors 각 축의 배율.
 */
[[nodiscard]]
constexpr Mat4 scale(const Vec3& factors) noexcept {
    Mat4 result;
    result(0, 0) = factors.x;
    result(1, 1) = factors.y;
    result(2, 2) = factors.z;
    return result;
}

/**
 * @brief 모든 축에 같은 배율을 적용하는 크기 행렬을 만든다.
 * @param[in] factor 균일 배율.
 */
[[nodiscard]]
constexpr Mat4 scale(float factor) noexcept {
    return scale(Vec3{factor});
}

/**
 * @brief X축 기준 회전 행렬을 만든다.
 * @param[in] radians 회전 각도. 라디안.
 */
[[nodiscard]]
inline Mat4 rotationX(float radians) noexcept {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result;
    result(1, 1) = cosine;
    result(1, 2) = -sine;
    result(2, 1) = sine;
    result(2, 2) = cosine;
    return result;
}

/**
 * @brief Y축 기준 회전 행렬을 만든다.
 * @param[in] radians 회전 각도. 라디안.
 */
[[nodiscard]]
inline Mat4 rotationY(float radians) noexcept {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result;
    result(0, 0) = cosine;
    result(0, 2) = sine;
    result(2, 0) = -sine;
    result(2, 2) = cosine;
    return result;
}

/**
 * @brief Z축 기준 회전 행렬을 만든다.
 * @param[in] radians 회전 각도. 라디안.
 */
[[nodiscard]]
inline Mat4 rotationZ(float radians) noexcept {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    Mat4 result;
    result(0, 0) = cosine;
    result(0, 1) = -sine;
    result(1, 0) = sine;
    result(1, 1) = cosine;
    return result;
}

/**
 * @brief Vulkan용 원근 투영 행렬을 만든다.
 *
 * 오른손 좌표계(카메라가 -Z를 봄) 규약이다. Vulkan NDC는 y가 아래로 증가하고
 * 깊이 범위가 [0, 1]이므로 y를 뒤집고 깊이를 0..1로 매핑한다.
 *
 * @param[in] fovYRadians 세로 시야각. 라디안. 0보다 크고 pi보다 작아야 한다.
 * @param[in] aspect      가로/세로 비율. 0보다 커야 한다.
 * @param[in] nearPlane   가까운 평면 거리. 0보다 커야 한다.
 * @param[in] farPlane    먼 평면 거리. nearPlane보다 커야 한다.
 */
[[nodiscard]]
inline Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) noexcept {
    const float focal = 1.0f / std::tan(fovYRadians * 0.5f);

    Mat4 result;
    result(0, 0) = focal / aspect;
    result(1, 1) = -focal;
    result(2, 2) = farPlane / (nearPlane - farPlane);
    result(2, 3) = (farPlane * nearPlane) / (nearPlane - farPlane);
    result(3, 2) = -1.0f;
    result(3, 3) = 0.0f;
    return result;
}

/**
 * @brief 오른손 좌표계 뷰 행렬을 만든다.
 *
 * eye에서 target을 바라본다. up은 시선과 평행하지 않아야 한다.
 * 퇴화한 입력을 검사하지 않는다.
 *
 * @param[in] eye    카메라 위치.
 * @param[in] target 바라보는 점.
 * @param[in] up     위쪽을 나타내는 방향.
 */
[[nodiscard]]
inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
    const Vec3 forward = normalized(target - eye);
    const Vec3 right = normalized(cross(forward, up));
    const Vec3 trueUp = cross(right, forward);

    Mat4 result;
    result(0, 0) = right.x;
    result(0, 1) = trueUp.x;
    result(0, 2) = -forward.x;

    result(1, 0) = right.y;
    result(1, 1) = trueUp.y;
    result(1, 2) = -forward.y;

    result(2, 0) = right.z;
    result(2, 1) = trueUp.z;
    result(2, 2) = -forward.z;

    result(0, 3) = -dot(right, eye);
    result(1, 3) = -dot(trueUp, eye);
    result(2, 3) = dot(forward, eye);

    return result;
}

} // namespace nxt::math
