#pragma once

#include <nxt/math/vec3.hpp>
#include <type_traits>
#include <vector>

namespace nxt::renderer {

/**
 * @brief 메시를 이루는 정점 하나이다.
 *
 * 위치와 색을 각각 Vec3로 가진다. Phase 0의 triangle.vert가
 * layout(location = 0) in vec3 inPosition, layout(location = 1) in vec3 inColor로
 * 받으므로 그 순서와 크기를 그대로 따른다.
 *
 * @note 패딩이 없이 연속으로 놓이므로 정점 배열을 그대로 매핑한 메모리에 복사할 수 있다.
 * @note Phase 0의 셰이더는 변환 행렬을 쓰지 않는다. position은 이미 clip 공간 좌표다.
 */
struct MeshVertex {
    /// @brief 정점 위치이다. Phase 0에서는 clip 공간 좌표다.
    math::Vec3 position;

    /// @brief 정점 색이다. 프래그먼트 셰이더가 그대로 출력한다.
    math::Vec3 color;
};

/// @brief 정점을 매핑한 메모리에 그대로 복사할 수 있어야 한다.
static_assert(std::is_trivially_copyable_v<MeshVertex>, "MeshVertex는 trivially copyable이어야 한다.");

/**
 * @brief CPU에 있는 메시의 정점 목록이다.
 *
 * Phase 0은 인덱스 없이 정점만 그린다. GPU 업로드는 triangle_pass가 맡고, 이 구조체는
 * GPU를 모른다 — 정점이 어디서 왔는지(하드코딩·파일)도 모른다.
 *
 * @note vertices를 소유한다. Renderer가 Mesh를 값으로 보관하므로 복사하면 정점 목록도
 *       함께 복사된다. 업로드가 끝나면 GPU 쪽은 이 목록을 참조하지 않는다.
 * @note Thread safety: thread-safe하지 않다. 업로드 전까지 한 스레드에서만 만진다.
 */
struct Mesh {
    /// @brief 이 메시의 정점들이다. 비어 있으면 그릴 것이 없다.
    std::vector<MeshVertex> vertices;
};

} // namespace nxt::renderer
