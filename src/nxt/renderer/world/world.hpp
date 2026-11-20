#pragma once

#include <cstddef>
#include <nxt/math/aabb.hpp>
#include <nxt/math/mat4.hpp>
#include <nxt/math/vec3.hpp>
#include <span>
#include <vector>

namespace nxt::renderer {

/**
 * @brief 한 프레임에 그릴 오브젝트 하나이다.
 *
 * Phase 1은 단위 삼각형 하나를 모든 오브젝트가 공유하고, 오브젝트마다 변환과 색만
 * push constant로 넘긴다. 정점 10만 개를 업로드하지 않는 이유는 측정 대상이
 * 정점 메모리 대역폭으로 바뀌지 않게 하기 위해서다 (phase1.md 1-A).
 *
 * bounds는 transform을 적용한 뒤의 축 정렬 경계 상자이다. 1-A1은 이것을 저장만
 * 하고, 1-A3의 프러스텀 컬링이 처음 소비한다.
 *
 * @note 값 타입이다. World가 vector로 소유한다.
 */
struct WorldObject {
    /// @brief 단위 삼각형에 적용할 변환이다. 서브스텝 1-A1은 clip 공간 좌표를 만든다.
    math::Mat4 transform{};

    /// @brief transform을 적용한 뒤의 축 정렬 경계 상자이다.
    math::Aabb bounds{};

    /// @brief 프래그먼트에 넘길 색이다. 알파는 1로 고정한다.
    math::Vec3 color{1.0f, 1.0f, 1.0f};
};

/**
 * @brief 프레임을 넘어 사는 렌더링 씬이다.
 *
 * 지속 상태(오브젝트 목록, 앞으로 GPU 핸들)를 소유한다. 프레임마다 다시 만드는
 * 카메라와 컬링 결과는 World가 아니라 FrameContext가 든다 (scene_world.md §5).
 * Phase 1은 engine/scene을 쓰지 않고 이 World만 쓴다.
 *
 * @note 수명을 가진 객체다. Engine이 소유하고 앱이 run() 전에 채운다.
 * @note Thread safety: thread-safe하지 않다. 채우는 스레드와 record하는 스레드가
 *       같아야 한다. Phase 1은 둘 다 메인 스레드다.
 */
class World final {
public:
    /**
     * @brief 오브젝트를 목록 끝에 더한다.
     *
     * 순서가 곧 draw 순서다. 같은 입력에서 같은 순서를 유지해야 벤치마크가
     * 재현된다 (phase1.md §5.4).
     *
     * @param[in] object 더할 오브젝트. 값으로 받아 이동해 보관한다.
     */
    void add(WorldObject object);

    /// @brief 지금까지 더한 오브젝트 수를 반환한다. draw call 수의 기준이다.
    [[nodiscard]]
    std::size_t objectCount() const noexcept;

    /// @brief draw 순서대로 정렬된 오브젝트 목록을 반환한다.
    [[nodiscard]]
    std::span<const WorldObject> objects() const noexcept;

private:
    /// @brief 추가된 순서를 그대로 유지하는 오브젝트 목록이다.
    std::vector<WorldObject> objects_;
};

} // namespace nxt::renderer
