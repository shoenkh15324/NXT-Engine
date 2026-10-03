#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace nxt::handle {

inline constexpr std::uint32_t kInvalidIndex = (std::numeric_limits<std::uint32_t>::max)();

/*
    세대(generation) 기반 핸들. 8바이트 값 타입이며 트리플이 아닌 소유자 없는 참조다.
    유효성은 index만으로 판정하고, 세대 비교는 소유자인 HandleManager가 담당한다.

    Thread safety: 값 타입이므로 복사와 비교에 lock이 필요 없다.
*/
template <typename Tag>
class Handle {
public:
    // 기본 생성 값이 곧 무효 핸들이므로 별도의 상태를 두지 않는다.
    constexpr Handle() noexcept = default;

    [[nodiscard]] static constexpr Handle invalid() noexcept {
        return {};
    }

    // 세대 공급자인 HandleManager 전용 경로다. 임의로 만든 핸들은 HandleManager::contains()에서 걸러진다.
    [[nodiscard]] static constexpr Handle make(std::uint32_t index, std::uint32_t generation) noexcept {
        return Handle{index, generation};
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index_ != kInvalidIndex;
    }

    [[nodiscard]] constexpr std::uint32_t index() const noexcept {
        return index_;
    }

    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return generation_;
    }

    constexpr auto operator<=>(const Handle&) const noexcept = default;

private:
    // 생성 경로를 make() 하나로 제한해 잘못된 index/generation 쌍을 막는다.
    constexpr Handle(std::uint32_t index, std::uint32_t generation) noexcept : index_{index}, generation_{generation} {}

    std::uint32_t index_{kInvalidIndex};
    std::uint32_t generation_{0}; // 0은 "한 번도 발행되지 않은 세대"로 예약한다. 슬롯은 1부터 시작한다.
};

} // namespace nxt::handle
