#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace nxt::core::handle {

/// @brief 슬롯 인덱스가 유효하지 않음을 나타내는 값이다.
inline constexpr std::uint32_t kInvalidIndex = (std::numeric_limits<std::uint32_t>::max)();

/**
 * @brief 세대(generation) 기반 핸들이다.
 *
 * 8바이트 값 타입이며 트리플이 아닌 소유자 없는 참조다.
 * 유효성은 index만으로 판정하고, 세대 비교는 소유자인 HandleManager가 담당한다.
 *
 * @note Thread safety: 값 타입이므로 복사와 비교에 lock이 필요 없다.
 *
 * @tparam Tag 서로 다른 핸들 종류를 구분하는 태그 타입이다.
 */
template <typename Tag>
class Handle {
public:
    /**
     * @brief 무효 핸들로 초기화한다.
     *
     * 기본 생성 값이 곧 무효 핸들이므로 별도의 상태를 두지 않는다.
     */
    constexpr Handle() noexcept = default;

    /**
     * @brief 무효 핸들을 반환한다.
     */
    [[nodiscard]]
    static constexpr Handle invalid() noexcept {
        return {};
    }

    /**
     * @brief 주어진 인덱스와 세대로 핸들을 만든다.
     *
     * 세대 공급자인 HandleManager 전용 경로다. 임의로 만든 핸들은
     * HandleManager::contains()에서 걸러진다.
     *
     * @param[in] index 슬롯 인덱스.
     * @param[in] generation 슬롯의 현재 세대.
     */
    [[nodiscard]]
    static constexpr Handle make(std::uint32_t index, std::uint32_t generation) noexcept {
        return Handle{index, generation};
    }

    /**
     * @brief 유효한 슬롯을 가리키는지 여부를 반환한다.
     */
    [[nodiscard]]
    constexpr bool valid() const noexcept {
        return index_ != kInvalidIndex;
    }

    /**
     * @brief 가리키는 슬롯 인덱스를 반환한다.
     */
    [[nodiscard]]
    constexpr std::uint32_t index() const noexcept {
        return index_;
    }

    /**
     * @brief 들고 있는 세대를 반환한다.
     */
    [[nodiscard]]
    constexpr std::uint32_t generation() const noexcept {
        return generation_;
    }

    /// 비교는 defaulted 연산자로부터 파생된다.
    constexpr auto operator<=>(const Handle&) const noexcept = default;

private:
    /**
     * @brief 인덱스와 세대로 핸들을 만든다.
     *
     * 생성 경로를 make() 하나로 제한해 잘못된 index/generation 쌍을 막는다.
     */
    constexpr Handle(std::uint32_t index, std::uint32_t generation) noexcept : index_{index}, generation_{generation} {}

    /// 가리키는 슬롯 인덱스다. kInvalidIndex이면 무효 핸들이다.
    std::uint32_t index_{kInvalidIndex};

    /// 0은 "한 번도 발행되지 않은 세대"로 예약한다. 슬롯은 1부터 시작한다.
    std::uint32_t generation_{0};
};

} // namespace nxt::core::handle
