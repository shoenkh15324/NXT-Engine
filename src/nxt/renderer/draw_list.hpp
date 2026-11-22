#pragma once

#include <cstddef>
#include <nxt/math/frustum.hpp>
#include <nxt/renderer/world/world.hpp>
#include <span>
#include <vector>

namespace nxt::renderer {

/**
 * @brief 이번 프레임에 그릴 visible 오브젝트 목록이다.
 *
 * World의 오브젝트를 가리키는 포인터만 담는다. 오브젝트를 복사하지 않으므로
 * 100k 목록을 매 프레임 다시 만들어도 값 비용이 들지 않는다.
 *
 * @note World의 오브젝트 주소를 가리킨다. World가 수정되거나 파괴된 뒤에 쓰면
 *       안 된다. 매 프레임 buildDrawList()로 다시 채운다.
 * @note 값 타입이다. Engine이 프레임을 넘겨 재사용하고 capacity를 유지한다.
 * @note Thread safety: thread-safe하지 않다. 만드는 스레드와 소비하는 스레드가
 *       같아야 한다. Phase 1은 둘 다 메인 스레드다.
 */
class DrawList final {
public:
    /// @brief 목록을 비운다. capacity는 유지한다.
    void clear() noexcept;

    /// @brief 오브젝트 하나를 목록 끝에 더한다. World 순서를 유지한다.
    void add(const WorldObject& object);

    /// @brief 담긴 오브젝트 수이다. 이번 프레임의 draw call 수와 같다.
    [[nodiscard]]
    std::size_t size() const noexcept;

    /// @brief 목록이 비었는지 여부이다.
    [[nodiscard]]
    bool empty() const noexcept;

    /// @brief draw 순서대로 정렬된 오브젝트 포인터 목록이다.
    [[nodiscard]]
    std::span<const WorldObject* const> objects() const noexcept;

private:
    /// @brief World의 오브젝트를 가리키는 포인터 목록이다.
    std::vector<const WorldObject*> objects_;
};

/**
 * @brief World를 한 번 훑어 이번 프레임의 visible 목록을 만든다.
 *
 * cullingEnabled가 false면 모든 오브젝트를, true면 frustum과 겹치는 오브젝트만
 * 담는다. 어느 쪽이든 World 순서를 유지하므로 draw 순서가 입력 순서와 같다.
 *
 * @param[in]  world          훑을 월드.
 * @param[in]  frustum        판정에 쓸 프러스텀. 보통 Frustum::clipSpace()이다.
 * @param[in]  cullingEnabled false면 컬링을 건너뛴다.
 * @param[out] output         채울 목록. 먼저 비운 뒤 World 순서대로 채운다.
 */
void buildDrawList(const World& world, const math::Frustum& frustum, bool cullingEnabled, DrawList& output);

} // namespace nxt::renderer
