#pragma once

#include <nxt/math/frustum.hpp>
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

/**
 * @brief AABB가 프러스텀과 겹치는지 판정한다.
 *
 * 각 평면에 대해 상자에서 법선 방향으로 가장 먼 꼭짓점을 고른다. 그 점이
 * 평면 바깥이면 상자 전체가 그 평면 바깥에 있으므로 겹치지 않는다. 모든
 * 평면에서 이 검사를 통과하면 내부이거나 경계를 걸친 것으로 본다.
 *
 * 이 판정은 보수적이다. 프러스텀 밖에 있지만 어떤 평면 하나의 완전한 바깥쪽에
 * 있지 않은 상자는 visible로 남는다. 컬링은 그려야 할 것을 지우지 않는 쪽이
 * 안전하므로 이 근사를 쓴다.
 *
 * @param[in] bounds  판정할 축 정렬 상자. 프러스텀과 같은 공간에 있어야 한다.
 * @param[in] frustum 판정에 쓸 프러스텀.
 * @return 상자가 프러스텀 안쪽이거나 경계를 걸치면 true, 완전히 바깥이면 false.
 */
[[nodiscard]]
constexpr bool intersects(const Aabb& bounds, const Frustum& frustum) noexcept {
    for (const Plane& plane : frustum.planes) {
        const Vec3 farthest{
            plane.normal.x >= 0.0f ? bounds.max.x : bounds.min.x,
            plane.normal.y >= 0.0f ? bounds.max.y : bounds.min.y,
            plane.normal.z >= 0.0f ? bounds.max.z : bounds.min.z,
        };

        if ((dot(plane.normal, farthest) + plane.distance) < 0.0f) {
            return false;
        }
    }

    return true;
}

} // namespace nxt::math
