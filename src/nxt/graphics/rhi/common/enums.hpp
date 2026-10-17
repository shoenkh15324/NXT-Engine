#pragma once

#include <cstdint>

namespace nxt::graphics::rhi {

/**
 * @brief 이미지의 픽셀을 저장하는 형식이다.
 *
 * 이름은 Vulkan 명세의 VkFormat를 그대로 옮긴 것이다. 비트가 어떻게 놓였는지가
 * 이름에 드러나므로 형식을 고를 때 추측할 필요가 없다. 번역 계층을 두지 않는
 * 이유는 rhi.md §3의 thin RHI 결정에 따른다.
 */
enum class Format : std::uint8_t {
    /// @brief 형식이 없거나 아직 정해지지 않았다. VkFormat::eUndefined에 대응한다.
    Undefined = 0,
    /// @brief 8비트씩 R, G, B, A 순서의 정규화 값. 범위는 [0, 1].
    R8G8B8A8Unorm,
    /// @brief 8비트씩 B, G, R, A 순서의 정규화 값. 범위는 [0, 1].
    ///
    /// Win32 스왑체인이 기본으로 주는 형식이다.
    B8G8R8A8Unorm,
    /// @brief 8비트씩 B, G, R, A 순서이며 sRGB 감응 곡선을 거친 값.
    B8G8R8A8Srgb,
    /// @brief 깊이 전용 32비트 부동소수점.
    D32SFloat,
    /// @brief 깊이 24비트 정규화와 스텐실 8비트.
    D24UnormS8Uint,
    /// @brief 깊이 32비트 부동소수점과 스텐실 8비트.
    D32SFloatS8Uint,
};

/**
 * @brief 스왑체인 이미지를 화면에 제시하는 방식이다.
 *
 * 기본값은 Fifo다 (frame.md §5). 지연이 문제가 될 때만 Mailbox로 바꾼다.
 */
enum class PresentMode : std::uint8_t {
    /// @brief 표시 신호에 맞춰 제시한다. 지연이 생기지만 화면이 찢어지지 않는다.
    Fifo = 0,
    /// @brief Fifo와 같지만 지연을 조금 줄인다.
    FifoRelaxed,
    /// @brief 가장 최근 이미지를 즉시 제시한다. 지연이 낮지만 VSync가 없다.
    Mailbox,
    /// @brief 즉시 제시한다. 지연이 가장 낮고 화면이 찢어질 수 있다.
    Immediate,
};

/**
 * @brief 렌더 패스가 시작될 때 attachment 내용을 어떻게 할지 정한다.
 */
enum class LoadOp : std::uint8_t {
    /// @brief 기존 내용을 유지한다.
    Load = 0,
    /// @brief clear 값으로 덮어쓴다.
    Clear,
    /// @brief 내용을 보존할 필요가 없으므로 건드리지 않는다.
    DontCare,
};

/**
 * @brief 렌더 패스가 끝난 뒤 attachment 내용을 어떻게 할지 정한다.
 */
enum class StoreOp : std::uint8_t {
    /// @brief 결과를 남긴다.
    Store = 0,
    /// @brief 결과를 남길 필요가 없으므로 버린다.
    DontCare,
};

/**
 * @brief 정점을 이어 붙여 만드는 기본 도형의 종류이다.
 */
enum class Topology : std::uint8_t {
    /// @brief 정점 하나마다 점 하나.
    PointList = 0,
    /// @brief 정점 두 개마다 선분 하나.
    LineList,
    /// @brief 정점을 차례로 이어 선분 여러 개.
    LineStrip,
    /// @brief 정점 세 개마다 삼각형 하나. NXT의 기본값이다.
    TriangleList,
    /// @brief 정점을 차례로 이어 삼각형 여러 개.
    TriangleStrip,
};

/**
 * @brief 인덱스 버퍼의 원소 하나가 몇 바이트인지 나타낸다.
 */
enum class IndexType : std::uint8_t {
    /// @brief 원소당 16비트 부호 없는 정수.
    U16 = 0,
    /// @brief 원소당 32비트 부호 없는 정수.
    U32,
};

/**
 * @brief 버퍼를 어떤 용도로 쓸지 나타내는 비트 집합이다.
 *
 * VkBufferUsageFlagBits를 옮긴 것이다. backend 구현부가 vk 플래그로 번역한다.
 */
enum class BufferUsage : std::uint32_t {
    /// @brief 용도를 지정하지 않는다. 읽지도 쓰지도 않는다.
    None = 0,
    /// @brief 정점 버퍼로 쓴다.
    Vertex = 1u << 0,
    /// @brief 인덱스 버퍼로 쓴다.
    Index = 1u << 1,
    /// @brief 셰이더의 균일 상수로 쓴다.
    Uniform = 1u << 2,
    /// @brief 셰이더의 저장 버퍼로 쓴다.
    Storage = 1u << 3,
    /// @brief 다른 리소스로 복사하는 원본이다.
    TransferSrc = 1u << 4,
    /// @brief 다른 리소스로 복사되는 대상이다.
    TransferDst = 1u << 5,
};

/**
 * @brief 두 용도를 합친다.
 */
[[nodiscard]]
constexpr BufferUsage operator|(BufferUsage lhs, BufferUsage rhs) noexcept {
    return static_cast<BufferUsage>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

/// @brief 기존 용도에 더한다.
constexpr BufferUsage& operator|=(BufferUsage& lhs, BufferUsage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

} // namespace nxt::graphics::rhi
