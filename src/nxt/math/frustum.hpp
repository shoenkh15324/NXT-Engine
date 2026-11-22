#pragma once

#include <array>
#include <nxt/math/vec3.hpp>

namespace nxt::math {

/**
 * @brief 프러스텀을 이루는 평면 하나이다.
 *
 * 평면의 안쪽은 dot(normal, point) + distance >= 0인 반공간이다. normal은
 * 바깥쪽을 가리키며, 단위 벡터이면 distance가 원점에서 평면까지의 부호 있는
 * 거리와 같은 의미를 갖는다.
 *
 * @note 값 타입이다. Frustum이 배열로 소유한다.
 */
struct Plane {
    /// @brief 평면의 바깥쪽을 가리키는 법선이다.
    Vec3 normal{};

    /// @brief 원점에서 평면까지의 부호 있는 거리이다.
    float distance = 0.0f;
};

/**
 * @brief 클립 공간에 고정된 프러스텀이다.
 *
 * Phase 1 1-A의 오브젝트 변환은 아핀이라 w가 1이고, clip 공간과 NDC가 같다.
 * 따라서 프러스텀은 고정된 상자 [-1, 1] x [-1, 1] x [0, 1]이 된다. Vulkan의
 * 깊이 범위가 [0, 1]이라 가까운 평면이 z = 0, 먼 평면이 z = 1이다.
 *
 * 평면 순서는 왼쪽, 오른쪽, 아래, 위, 가까운, 먼이다.
 *
 * @note 값 타입이다. 매 프레임 다시 만들 필요가 없어 상수로 둘 수 있다.
 */
struct Frustum {
    /// @brief 6개 평면이다. 안쪽은 모든 평면의 양의 반공간이다.
    std::array<Plane, 6> planes{};

    /**
     * @brief 클립 공간의 고정 프러스텀을 만든다.
     * @return 위에서 설명한 상자와 같은 6평면 프러스텀.
     */
    [[nodiscard]]
    static constexpr Frustum clipSpace() noexcept {
        return Frustum{{
            Plane{Vec3{1.0f, 0.0f, 0.0f}, 1.0f},  // 왼쪽:    x + 1 >= 0
            Plane{Vec3{-1.0f, 0.0f, 0.0f}, 1.0f}, // 오른쪽: -x + 1 >= 0
            Plane{Vec3{0.0f, 1.0f, 0.0f}, 1.0f},  // 아래:    y + 1 >= 0
            Plane{Vec3{0.0f, -1.0f, 0.0f}, 1.0f}, // 위:     -y + 1 >= 0
            Plane{Vec3{0.0f, 0.0f, 1.0f}, 0.0f},  // 가까운:   z >= 0
            Plane{Vec3{0.0f, 0.0f, -1.0f}, 1.0f}, // 먼:     -z + 1 >= 0
        }};
    }
};

} // namespace nxt::math
