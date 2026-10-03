#pragma once

#include <cstddef>
#include <cstdint>
#include <nxt/core/handle/handle.hpp>
#include <vector>

namespace nxt::handle {

/*
    슬롯별 세대와 free list를 소유한다. 객체 자체는 저장하지 않으며, 메모리 계층이 같은 슬롯 인덱스로 객체를 보관한다.

    Thread safety: thread-safe하지 않다. 한 소유자가 단독으로 접근해야 하며,
    공유가 필요하면 호출자가 외부에서 직렬화한다.
*/
template <typename Tag>
class HandleManager {
public:
    using HandleType = Handle<Tag>;

    // 슬롯이 고갈되면 invalid()를 반환한다. 슬롯 하나가 12바이트라 kInvalidIndex개는
    // 48GB를 넘겨 현실적으로 도달하지 않는다.
    // 빈 슬롯을 재사용할 때는 세대를 그대로 쓰므로 이전 핸들이 자동으로 stale가 된다.
    [[nodiscard]] HandleType create() {
        if (freeHead_ != kInvalidIndex) {
            const std::uint32_t index = freeHead_;
            Slot& slot = slots_[index];
            freeHead_ = slot.next;
            slot.next = kInvalidIndex;
            slot.alive = true;
            return HandleType::make(index, slot.generation);
        }

        if (slots_.size() >= kInvalidIndex) {
            return HandleType::invalid();
        }

        const auto index = static_cast<std::uint32_t>(slots_.size());
        slots_.push_back(Slot{});
        slots_[index].alive = true;
        return HandleType::make(index, slots_[index].generation);
    }

    // 해제된 슬롯의 세대를 증가시키므로 이전 핸들은 더 이상 유효하지 않다.
    // 슬롯에 닿기 전에 contains()로 범위와 생존을 확인한다.
    bool destroy(HandleType handle) noexcept {
        if (!contains(handle)) {
            return false;
        }

        Slot& slot = slots_[handle.index()];

        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }

        slot.alive = false;
        slot.next = freeHead_;
        freeHead_ = handle.index();
        return true;
    }

    // 세대만 비교하면 free list에 있는 슬롯을 가리키는 위조 핸들이 통과하므로
    // alive까지 함께 확인한다.
    [[nodiscard]] bool contains(HandleType handle) const noexcept {
        if (!handle.valid()) {
            return false;
        }

        const std::size_t index = handle.index();
        if (index >= slots_.size()) {
            return false;
        }

        const Slot& slot = slots_[index];
        return slot.alive && (slot.generation == handle.generation());
    }

private:
    struct Slot {
        std::uint32_t generation{1};       // Handle의 기본 generation(0)과 겹치지 않게 1부터 시작한다.
        std::uint32_t next{kInvalidIndex}; // free list의 다음 슬롯. 리스트의 끝은 kInvalidIndex.
        bool alive{false};                 // 세대와 짝을 이루는 생존 여부.
    };

    std::vector<Slot> slots_;
    std::uint32_t freeHead_{kInvalidIndex}; // free list의 머리. 비어 있으면 kInvalidIndex.
};

} // namespace nxt::handle
