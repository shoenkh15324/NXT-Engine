#pragma once

#include <cmath>

namespace nxt::math {

/**
 * @brief 3차원 벡터이다.
 *
 * 세 성분을 float로 가지며 정점 위치·방향 계산에 쓴다. 크기가 12바이트라
 * 정점 버퍼에 그대로 올릴 수 있다.
 */
struct Vec3 {
    float x = 0.0f; ///< x 성분
    float y = 0.0f; ///< y 성분
    float z = 0.0f; ///< z 성분

    /// 영 벡터를 만든다.
    constexpr Vec3() noexcept = default;

    /**
     * @brief 세 성분을 모두 같은 값으로 채운다.
     * @param[in] scalar 각 성분에 넣을 값.
     */
    constexpr explicit Vec3(float scalar) noexcept : x{scalar}, y{scalar}, z{scalar} {}

    /**
     * @brief 성분을 각각 지정해 만든다.
     * @param[in] xValue x 성분.
     * @param[in] yValue y 성분.
     * @param[in] zValue z 성분.
     */
    constexpr Vec3(float xValue, float yValue, float zValue) noexcept : x{xValue}, y{yValue}, z{zValue} {}

    /// rhs를 성분별로 더한다.
    constexpr Vec3& operator+=(const Vec3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    /// rhs를 성분별로 뺀다.
    constexpr Vec3& operator-=(const Vec3& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    /// 모든 성분에 scalar를 곱한다.
    constexpr Vec3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    /**
     * @brief 모든 성분을 scalar로 나눈다.
     * @param[in] scalar 0이 아니어야 한다. 나눗셈을 검사하지 않는다.
     */
    constexpr Vec3& operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }
};

/// 성분별 덧셈 결과를 반환한다.
[[nodiscard]]
constexpr Vec3 operator+(Vec3 lhs, const Vec3& rhs) noexcept {
    return lhs += rhs;
}

/// 성분별 뺄셈 결과를 반환한다.
[[nodiscard]]
constexpr Vec3 operator-(Vec3 lhs, const Vec3& rhs) noexcept {
    return lhs -= rhs;
}

/// 방향을 뒤집은 벡터를 반환한다.
[[nodiscard]]
constexpr Vec3 operator-(Vec3 value) noexcept {
    return {-value.x, -value.y, -value.z};
}

/// 모든 성분에 scalar를 곱한 벡터를 반환한다.
[[nodiscard]]
constexpr Vec3 operator*(Vec3 value, float scalar) noexcept {
    return value *= scalar;
}

/// 모든 성분에 scalar를 곱한 벡터를 반환한다.
[[nodiscard]]
constexpr Vec3 operator*(float scalar, Vec3 value) noexcept {
    return value *= scalar;
}

/// 모든 성분을 scalar로 나눈 벡터를 반환한다.
[[nodiscard]]
constexpr Vec3 operator/(Vec3 value, float scalar) noexcept {
    return value /= scalar;
}

/// 두 벡터의 내적을 반환한다.
[[nodiscard]]
constexpr float dot(const Vec3& lhs, const Vec3& rhs) noexcept {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

/// 두 벡터의 외적을 반환한다. 오른손 좌표계 기준이다.
[[nodiscard]]
constexpr Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {(lhs.y * rhs.z) - (lhs.z * rhs.y), (lhs.z * rhs.x) - (lhs.x * rhs.z), (lhs.x * rhs.y) - (lhs.y * rhs.x)};
}

/// 벡터 길이의 제곱을 반환한다. 제곱근이 필요 없는 비교에 쓴다.
[[nodiscard]]
constexpr float lengthSquared(const Vec3& value) noexcept {
    return dot(value, value);
}

/**
 * @brief 벡터의 길이를 반환한다.
 * @note std::sqrt를 쓰므로 C++20에서는 constexpr 함수가 아니다.
 */
[[nodiscard]]
inline float length(const Vec3& value) noexcept {
    return std::sqrt(lengthSquared(value));
}

/**
 * @brief 길이가 1인 벡터를 반환한다.
 * @param[in] value 길이가 0이 아니어야 한다. 검사하지 않는다.
 */
[[nodiscard]]
inline Vec3 normalized(const Vec3& value) noexcept {
    return value / length(value);
}

} // namespace nxt::math
