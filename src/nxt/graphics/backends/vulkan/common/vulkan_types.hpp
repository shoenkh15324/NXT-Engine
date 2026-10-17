#pragma once

#include <cstdint>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/resource/memory.hpp>
#include <vulkan/vulkan.hpp>

// rhi 타입과 Vulkan 타입 사이의 유일한 번역 지점이다.
// vulkan/vulkan.hpp를 include하는 파일은 이 파일 하나뿐이며, rhi/ 밖으로 vk::가
// 새지 않도록 하는 경계가 여기서 강제된다 (rhi.md §4).

namespace nxt::graphics::vulkan {

/**
 * @brief rhi::Format을 대응하는 vk::Format으로 옮긴다.
 *
 * 배열 조회가 아니라 switch를 쓰는 이유는 /W4가 case 누락을 경고로 드러내기
 * 때문이다. 열거형을 늘렸는데 매핑을 빠뜨리면 배포 전에 보인다.
 */
[[nodiscard]]
inline vk::Format toVulkan(rhi::Format format) noexcept {
    switch (format) {
        case rhi::Format::Undefined:
            return vk::Format::eUndefined;
        case rhi::Format::R8G8B8A8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case rhi::Format::B8G8R8A8Unorm:
            return vk::Format::eB8G8R8A8Unorm;
        case rhi::Format::B8G8R8A8Srgb:
            return vk::Format::eB8G8R8A8Srgb;
        case rhi::Format::D32SFloat:
            return vk::Format::eD32Sfloat;
        case rhi::Format::D24UnormS8Uint:
            return vk::Format::eD24UnormS8Uint;
        case rhi::Format::D32SFloatS8Uint:
            return vk::Format::eD32SfloatS8Uint;
    }

    NXT_ASSERT(false && "rhi::Format 값이 매핑되지 않았다.");
    return vk::Format::eUndefined;
}

/// @brief rhi::PresentMode을 대응하는 vk::PresentModeKHR로 옮긴다.
[[nodiscard]]
inline vk::PresentModeKHR toVulkan(rhi::PresentMode mode) noexcept {
    switch (mode) {
        case rhi::PresentMode::Fifo:
            return vk::PresentModeKHR::eFifo;
        case rhi::PresentMode::FifoRelaxed:
            return vk::PresentModeKHR::eFifoRelaxed;
        case rhi::PresentMode::Mailbox:
            return vk::PresentModeKHR::eMailbox;
        case rhi::PresentMode::Immediate:
            return vk::PresentModeKHR::eImmediate;
    }

    NXT_ASSERT(false && "rhi::PresentMode 값이 매핑되지 않았다.");
    return vk::PresentModeKHR::eFifo;
}

/// @brief rhi::LoadOp을 대응하는 vk::AttachmentLoadOp으로 옮긴다.
[[nodiscard]]
inline vk::AttachmentLoadOp toVulkan(rhi::LoadOp op) noexcept {
    switch (op) {
        case rhi::LoadOp::Load:
            return vk::AttachmentLoadOp::eLoad;
        case rhi::LoadOp::Clear:
            return vk::AttachmentLoadOp::eClear;
        case rhi::LoadOp::DontCare:
            return vk::AttachmentLoadOp::eDontCare;
    }

    NXT_ASSERT(false && "rhi::LoadOp 값이 매핑되지 않았다");
    return vk::AttachmentLoadOp::eDontCare;
}

/// @brief rhi::StoreOp을 대응하는 vk::AttachmentStoreOp으로 옮긴다.
[[nodiscard]]
inline vk::AttachmentStoreOp toVulkan(rhi::StoreOp op) noexcept {
    switch (op) {
        case rhi::StoreOp::Store:
            return vk::AttachmentStoreOp::eStore;
        case rhi::StoreOp::DontCare:
            return vk::AttachmentStoreOp::eDontCare;
    }

    NXT_ASSERT(false && "rhi::StoreOp 값이 매핑되지 않았다");
    return vk::AttachmentStoreOp::eDontCare;
}

/// @brief rhi::Topology을 대응하는 vk::PrimitiveTopology로 옮긴다.
[[nodiscard]]
inline vk::PrimitiveTopology toVulkan(rhi::Topology topology) noexcept {
    switch (topology) {
        case rhi::Topology::PointList:
            return vk::PrimitiveTopology::ePointList;
        case rhi::Topology::LineList:
            return vk::PrimitiveTopology::eLineList;
        case rhi::Topology::LineStrip:
            return vk::PrimitiveTopology::eLineStrip;
        case rhi::Topology::TriangleList:
            return vk::PrimitiveTopology::eTriangleList;
        case rhi::Topology::TriangleStrip:
            return vk::PrimitiveTopology::eTriangleStrip;
    }

    NXT_ASSERT(false && "rhi::Topology 값이 매핑되지 않았다");
    return vk::PrimitiveTopology::eTriangleList;
}

/// @brief rhi::IndexType을 대응하는 vk::IndexType으로 옮긴다.
[[nodiscard]]
inline vk::IndexType toVulkan(rhi::IndexType type) noexcept {
    switch (type) {
        case rhi::IndexType::U16:
            return vk::IndexType::eUint16;
        case rhi::IndexType::U32:
            return vk::IndexType::eUint32;
    }

    NXT_ASSERT(false && "rhi::IndexType 값이 매핑되지 않았다");
    return vk::IndexType::eUint32;
}

/**
 * @brief rhi::BufferUsage 비트 집합을 vk::BufferUsageFlags로 옮긴다.
 *
 * 1:1 캐스트가 불가능하다. rhi와 vk의 비트 위치가 다르기 때문이다.
 * 예로 rhi::BufferUsage::TransferSrc는 1u << 4지만 VK_BUFFER_USAGE_TRANSFER_SRC_BIT는
 * 0x1이다. 값이 아니라 의미로 매핑해야 한다.
 */
[[nodiscard]]
inline vk::BufferUsageFlags toVulkan(rhi::BufferUsage usage) noexcept {
    const auto bits = static_cast<std::uint32_t>(usage);
    vk::BufferUsageFlags flags;

    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::Vertex)) {
        flags |= vk::BufferUsageFlagBits::eVertexBuffer;
    }
    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::Index)) {
        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::Uniform)) {
        flags |= vk::BufferUsageFlagBits::eUniformBuffer;
    }
    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::Storage)) {
        flags |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::TransferSrc)) {
        flags |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    if (bits & static_cast<std::uint32_t>(rhi::BufferUsage::TransferDst)) {
        flags |= vk::BufferUsageFlagBits::eTransferDst;
    }

    return flags;
}

/**
 * @brief rhi::MemoryProperty 비트 집합을 vk::MemoryPropertyFlags로 옮긴다.
 *
 * BufferUsage와 같은 이유로 값이 아니라 의미로 매핑한다. rhi::MemoryProperty의
 * HostCached는 1u << 2지만 대응하는 VK_MEMORY_PROPERTY_HOST_CACHED_BIT는 0x8이다.
 */
[[nodiscard]]
inline vk::MemoryPropertyFlags toVulkan(rhi::MemoryProperty property) noexcept {
    const auto bits = static_cast<std::uint32_t>(property);
    vk::MemoryPropertyFlags flags;

    if (bits & static_cast<std::uint32_t>(rhi::MemoryProperty::DeviceLocal)) {
        flags |= vk::MemoryPropertyFlagBits::eDeviceLocal;
    }
    if (bits & static_cast<std::uint32_t>(rhi::MemoryProperty::HostVisible)) {
        flags |= vk::MemoryPropertyFlagBits::eHostVisible;
    }
    if (bits & static_cast<std::uint32_t>(rhi::MemoryProperty::HostCached)) {
        flags |= vk::MemoryPropertyFlagBits::eHostCached;
    }
    if (bits & static_cast<std::uint32_t>(rhi::MemoryProperty::HostCoherent)) {
        flags |= vk::MemoryPropertyFlagBits::eHostCoherent;
    }

    return flags;
}

/**
 * @brief ResourceHandle이 담은 값을 vk 핸들로 되돌린다.
 *
 * 64비트에서 Vulkan 핸들은 디스패치 가능 여부와 상관없이 모두 포인터 타입이다
 * (VK_USE_64_BIT_PTR_DEFINES == 1 — vulkan_core.h). 그래서 정수에서 핸들로 가려면
 * reinterpret_cast를 써야 한다. static_cast로는 컴파일되지 않는다.
 *
 * @tparam VkHandle 되돌릴 vk 핸들 타입.
 * @tparam Tag      핸들의 종류 태그.
 * @param[in] handle 되돌릴 rhi 핸들.
 * @return 되돌린 vk 핸들.
 */
template <typename VkHandle, typename Tag>
[[nodiscard]]
inline VkHandle toVkHandle(rhi::ResourceHandle<Tag> handle) noexcept {
    return VkHandle{reinterpret_cast<typename VkHandle::CType>(handle.value())};
}

/**
 * @brief vk 핸들을 종류가 구분되는 rhi 핸들로 감싼다.
 *
 * 핸들을 64비트 값으로 보관한다. 포인터를 정수로 바꾸는 길은 reinterpret_cast
 * 뿐이다 (toVkHandle과 같은 이유).
 *
 * @tparam Tag      감쌀 핸들의 종류 태그.
 * @tparam VkHandle 감쌀 vk 핸들 타입.
 * @param[in] handle 감쌀 vk 핸들.
 * @return 감싼 rhi 핸들.
 */
template <typename Tag, typename VkHandle>
[[nodiscard]]
inline rhi::ResourceHandle<Tag> fromVkHandle(VkHandle handle) noexcept {
    return rhi::ResourceHandle<Tag>::fromValue(
        reinterpret_cast<std::uint64_t>(static_cast<typename VkHandle::CType>(handle)));
}

} // namespace nxt::graphics::vulkan
