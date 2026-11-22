#include <doctest/doctest.h>
#include <nxt/math/aabb.hpp>
#include <nxt/math/frustum.hpp>

namespace {

constexpr nxt::math::Frustum kFrustum = nxt::math::Frustum::clipSpace();

[[nodiscard]]
constexpr nxt::math::Aabb box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) noexcept {
    return nxt::math::Aabb{
        .min = {minX, minY, minZ},
        .max = {maxX, maxY, maxZ},
    };
}

} // namespace

TEST_CASE("프러스텀 내부의 AABB는 겹친다") {
    constexpr auto bounds = box(-0.5f, -0.5f, 0.25f, 0.5f, 0.5f, 0.75f);

    CHECK(nxt::math::intersects(bounds, kFrustum));
}

TEST_CASE("프러스텀 외부의 AABB는 겹치지 않는다.") {
    // 여섯 평면 각각의 바깥으로 완전히 벗어난 상자이다.
    constexpr nxt::math::Aabb outside[]{
        box(1.5f, -0.5f, 0.25f, 2.0f, 0.5f, 0.75f),   // 오른쪽
        box(-2.0f, -0.5f, 0.25f, -1.5f, 0.5f, 0.75f), // 왼쪽
        box(-0.5f, 1.5f, 0.25f, 0.5f, 2.0f, 0.75f),   // 위
        box(-0.5f, -2.0f, 0.25f, 0.5f, -1.5f, 0.75f), // 아래
        box(-0.5f, -0.5f, -1.0f, 0.5f, 0.5f, -0.5f),  // 가까운 평면보다 뒤
        box(-0.5f, -0.5f, 1.5f, 0.5f, 0.5f, 2.0f),    // 먼 평면보다 앞}
    };

    for (const nxt::math::Aabb& bounds : outside) {
        CHECK_FALSE(nxt::math::intersects(bounds, kFrustum));
    }
}

TEST_CASE("프러스텀 경계를 걸친 AABB는 겹친다.") {
    constexpr auto straddling = box(0.5f, -0.5f, 0.25f, 1.5f, 0.5f, 0.75f); // x = 1을 가로지른다.
    CHECK(nxt::math::intersects(straddling, kFrustum));

    constexpr auto touching = box(-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f); // 여섯 평면에 정확히 닿는다.
    CHECK(nxt::math::intersects(touching, kFrustum));
}
