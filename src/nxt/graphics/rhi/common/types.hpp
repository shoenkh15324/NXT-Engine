#pragma once

#include <cstdint>

namespace nxt::graphics::rhi {

/// @brief 렌더 타겟이나 이미지의 크기이다. 픽셀 단위이다.
struct Extent2D {
    /// @brief 가로 길이.
    std::uint32_t width = 0;

    /// @brief 세로 길이.
    std::uint32_t height = 0;
};

} // namespace nxt::graphics::rhi
