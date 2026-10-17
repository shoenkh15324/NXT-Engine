#pragma once

#include <compare>
#include <cstdint>

namespace nxt::graphics::rhi {

/**
 * @brief 백엔드 리소스를 가리키는 의미 없는 64비트 값이다.
 *
 * Vulkan 핸들은 64비트로 표현되는 불투명한 값이다. 64비트에서는 디스패치 가능
 * 여부와 상관없이 포인터 타입이므로 정수로 보관하려면 변환이 필요하다
 * (backends/vulkan/vulkan_types.hpp). NXT는 그 값이 무엇을 뜻하는지 알지 않으며,
 * 해석은 backend 구현부에만 둔다 (rhi.md §4).
 *
 * @note 리소스 종류를 표현하려면 이 타입이 아니라 ResourceHandle<Tag>를 쓴다.
 */
using OpaqueHandle = std::uint64_t;

/**
 * @brief 종류가 구분되는 리소스 핸들이다.
 *
 * Vulkan은 VkImageView와 VkBuffer를 서로 다른 타입으로 선언하지만 실제로는 둘 다
 * 64비트 값이다. 값만 다루면 실수를 컴파일 시점에 잡을 수 없다. 이 래퍼는 종류를
 * 나타내는 빈 태그를 넣어 그 상실을 막는다.
 *
 * 값 0은 "유효하지 않음"이다. Vulkan 핸들이 그런 값을 돌려주는 일은 없다.
 *
 * @note Thread safety: 값 타입이다. 복사와 비교에 lock이 필요 없다.
 * @note 소유권이 없다. 파괴는 backend가 fence 신호 뒤에 결정한 시점에 일어난다
 *       (rhi.md §9).
 *
 * @tparam Tag 핸들 종류를 구분하는 빈 구조체다.
 */
template <typename Tag>
class ResourceHandle {
public:
    /**
     * @brief 유효하지 않은 핸들로 초기화한다.
     *
     * 기본 생성 값이 곧 유효하지 않음이라 별도의 상태를 두지 않는다.
     */
    constexpr ResourceHandle() noexcept = default;

    /**
     * @brief 백엔드가 반환한 원시 값으로 핸들을 만든다.
     *
     * 이 경로는 backend 구현부 전용이다. 임의로 만든 핸들은 해석한 뒤의 리소스가
     * 존재하지 않으므로 사용할 수 없다.
     *
     * @param[in] value 백엔드가 반환한 핸들 값. 0이면 유효하지 않은 핸들.
     */
    [[nodiscard]]
    static constexpr ResourceHandle fromValue(OpaqueHandle value) noexcept {
        return ResourceHandle{value};
    }

    /// @brief 유효한 핸들인지 여부를 반환한다.
    [[nodiscard]]
    constexpr bool valid() const noexcept {
        return value_ != 0;
    }

    /**
     * @brief 핸들이 담고 있는 원시 값을 반환한다.
     *
     * backend 구현부가 vk 타입으로 되돌릴 때만 쓴다.
     *
     * @return 핸들의 원시 값.
     */
    [[nodiscard]]
    constexpr OpaqueHandle value() const noexcept {
        return value_;
    }

    /// 비교는 defaulted 연산자로부터 파생된다.
    constexpr auto operator<=>(const ResourceHandle&) const noexcept = default;

private:
    /**
     * @brief 원시 값으로 핸들을 만든다.
     *
     * 생성 경로를 fromValue() 하나로 제한해 잘못된 값을 집어넣지 못하게 한다.
     */
    explicit constexpr ResourceHandle(OpaqueHandle value) noexcept : value_{value} {}

    /// 백엔드가 반환한 핸들 값, 0이면 유효하지 않다.
    OpaqueHandle value_{0};
};

} // namespace nxt::graphics::rhi
