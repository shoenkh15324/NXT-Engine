#include <nxt/math/aabb.hpp>
#include <nxt/renderer/draw_list.hpp>

namespace nxt::renderer {

void DrawList::clear() noexcept {
    objects_.clear();
}

void DrawList::add(const WorldObject& object) {
    objects_.push_back(&object);
}

std::size_t DrawList::size() const noexcept {
    return objects_.size();
}

bool DrawList::empty() const noexcept {
    return objects_.empty();
}

std::span<const WorldObject* const> DrawList::objects() const noexcept {
    return std::span<const WorldObject* const>{objects_.data(), objects_.size()};
}

void buildDrawList(const World& world, const math::Frustum& frustum, bool cullingEnabled, DrawList& output) {
    output.clear();

    for (const WorldObject& object : world.objects()) {
        if (!cullingEnabled || math::intersects(object.bounds, frustum)) {
            output.add(object);
        }
    }
}

} // namespace nxt::renderer
