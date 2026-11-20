#include <nxt/renderer/world/world.hpp>
#include <utility>

namespace nxt::renderer {

void World::add(WorldObject object) {
    objects_.push_back(std::move(object));
}

std::size_t World::objectCount() const noexcept {
    return objects_.size();
}

std::span<const WorldObject> World::objects() const noexcept {
    return objects_;
}

} // namespace nxt::renderer
