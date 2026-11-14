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
    /// @brief 32비트 부동소수점 두 개. 정점 속성에 쓴다. 범위는 [0, 1]이 아니다.
    R32G32Sfloat,
    /// @brief 32비트 부동소수점 세 개. 정점 위치에 쓴다. 범위는 [0, 1]이 아니다.
    R32G32B32Sfloat,
    /// @brief 32비트 부동소수점 네 개. 정점 속성에 쓴다. 범위는 [0, 1]이 아니다.
    R32G32B32A32Sfloat,
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
 * @brief 삼각형의 면을 어느 쪽에서 볼 때 버릴지 정한다.
 *
 * VkCullModeFlagBits를 옮긴 것이다. NXT는 면 하나만 버린다 — 앞뒤를 동시에
 * 버리는 조합은 쓰지 않는다.
 */
enum class CullMode : std::uint8_t {
    /// @brief 어느 면도 버리지 않는다. 파이프라인을 만들 때는 빈 플래그가 된다.
    None = 0,
    /// @brief 앞면을 버린다.
    Front,
    /// @brief 뒷면을 버린다. NXT의 기본값이다.
    Back,
};

/// @brief 정점이 감기는 두 방향 중 어느 쪽을 앞면으로 볼지 정한다.
enum class FrontFace : std::uint8_t {
    /// @brief 반시계 방향으로 감긴 면을 앞면으로 본다. Vulkan의 기본값이다.
    CounterClockwise = 0,
    /// @brief 시계 방향으로 감긴 면을 앞면으로 본다.
    Clockwise,
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

/**
 * @brief 이미지를 어떤 용도로 쓸지 나타내는 비트 집합이다.
 *
 * VkImageUsageFlagBits를 옮긴 것이다. backend 구현부가 vk 플래그로 번역한다.
 * 비트 위치는 BufferUsage와 같게 두었다 — 두 집합을 오갈 때 어느 비트가 전송인지
 * 다시 세지 않아도 된다.
 */
enum class ImageUsage : std::uint32_t {
    /// @brief 용도를 지정하지 않는다. 읽지도 쓰지도 않는다.
    None = 0,
    /// @brief 색 부착물로 쓴다.
    ColorAttachment = 1u << 0,
    /// @brief 깊이·스텐실 부착물로 쓴다.
    DepthStencilAttachment = 1u << 1,
    /// @brief 셰이더에서 텍스처로 읽는다.
    Sampled = 1u << 2,
    /// @brief 셰이더에서 저장 이미지로 읽고 쓴다.
    Storage = 1u << 3,
    /// @brief 다른 리소스로 복사하는 원본이다.
    TransferSrc = 1u << 4,
    /// @brief 다른 리소스로 복사되는 대상이다.
    TransferDst = 1u << 5,
};

/// @brief 두 용도를 합친다.
[[nodiscard]]
constexpr ImageUsage operator|(ImageUsage lhs, ImageUsage rhs) noexcept {
    return static_cast<ImageUsage>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

/// @brief 기존 용도에 더한다.
constexpr ImageUsage& operator|=(ImageUsage& lhs, ImageUsage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

/**
 * @brief 깊이 테스트가 새 조각의 깊이를 버퍼의 값과 비교하는 방법이다.
 *
 * VkCompareOp를 그대로 옮겼다. 깊이 테스트가 켜져 있으면 비교에 통과한 조각만
 * 색 부착물에 쓰이고, 통과한 조각의 깊이는 DepthState::writeEnabled에 따라 버퍼를
 * 갱신한다.
 *
 * NXT의 관례는 작은 z가 앞이다 — clip 공간에서 z가 작을수록 카메라에 가깝다.
 * 그래서 기본값인 Less가 "앞에 있는 것이 이긴다"를 뜻한다.
 */
enum class CompareOp : std::uint8_t {
    /// @brief 항상 실패한다. 테스트를 끄려면 이 값이 아니라 testEnabled를 끈다.
    Never = 0,
    /// @brief 새 깊이가 버퍼보다 작을 때 통과한다.
    Less,
    /// @brief 새 깊이가 버퍼와 같을 때 통과한다.
    Equal,
    /// @brief 새 깊이가 버퍼보다 작거나 같을 때 통과한다.
    LessOrEqual,
    /// @brief 새 깊이가 버퍼보다 클 때 통과한다.
    Greater,
    /// @brief 새 깊이가 버퍼와 다를 때 통과한다.
    NotEqual,
    /// @brief 새 깊이가 버퍼보다 크거나 같을 때 통과한다.
    GreaterOrEqual,
    /// @brief 항상 통과한다. 나중에 그린 것이 앞에 온다.
    Always,
};

} // namespace nxt::graphics::rhi
