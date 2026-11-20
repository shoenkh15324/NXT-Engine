#pragma once

#include <nxt/math/mat4.hpp>
#include <nxt/math/vec3.hpp>

namespace nxt::math {

/**
 * @brief 축 정렬 경계 상자이다.
 *
 * min과 max는 같은 공간의 좌표이다. Phase 1 1-A1의 오브젝트 변환은 clip 공간
 * 좌표를 만들므로 이 상자도 clip 공간에 있다. 1-A3에서 카메라가 들어오면 같은
 * 구조체를 카메라 공간 또는 월드 공간의 경계로 쓴다.
 *
 * @note 값 타입이다. 오브젝트 하나가 자기 경계를 값으로 들고 있는다.
 */
struct Aabb {
    /// @brief 각 축의 최솟값이다.
    Vec3 min{};

    /// @brief 각 축의 최댓값이다.
    Vec3 max{};
};

/**
 * @brief 상자의 여덟 꼭짓점을 변환해 새 축 정렬 경계 상자를 만든다.
 *
 * 회전이 섞이면 변환된 상자는 더 이상 축 정렬이 아니므로, 꼭짓점을 모두 변환한
 * 뒤 각 축의 최솟값과 최댓값을 다시 구한다. Phase 1 1-A1의 변환은 이동과 균일
 * 크기뿐이라 결과가 정확하다.
 *
 * @param[in] bounds 변환할 상자.
 * @param[in] matrix 적용할 변환. w가 0인 투영 변환은 넘기지 않는다.
 * @return 변환된 꼭짓점을 감싸는 축 정렬 상자.
 */
[[nodiscard]]
constexpr Aabb transformed(const Aabb& bounds, const Mat4& matrix) noexcept {
    const Vec3 corners[8]{
        {bounds.min.x, bounds.min.y, bounds.min.z}, {bounds.max.x, bounds.min.y, bounds.min.z},
        {bounds.min.x, bounds.max.y, bounds.min.z}, {bounds.max.x, bounds.max.y, bounds.min.z},
        {bounds.min.x, bounds.min.y, bounds.max.z}, {bounds.max.x, bounds.min.y, bounds.max.z},
        {bounds.min.x, bounds.max.y, bounds.max.z}, {bounds.max.x, bounds.max.y, bounds.max.z},
    };

    Vec3 minimum = transformPoint(matrix, corners[0]);
    Vec3 maximum = minimum;

    for (const Vec3& corner : corners) {
        const Vec3 point = transformPoint(matrix, corner);

        if (point.x < minimum.x) {
            minimum.x = point.x;
        }
        if (point.y < minimum.y) {
            minimum.y = point.y;
        }
        if (point.z < minimum.z) {
            minimum.z = point.z;
        }
        if (point.x > maximum.x) {
            maximum.x = point.x;
        }
        if (point.y > maximum.y) {
            maximum.y = point.y;
        }
        if (point.z > maximum.z) {
            maximum.z = point.z;
        }
    }

    return {.min = minimum, .max = maximum};
}

} // namespace nxt::math
