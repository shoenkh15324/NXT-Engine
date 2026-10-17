#pragma once

#include <memory>
#include <nxt/graphics/backends/vulkan/common/vulkan_types.hpp>
#include <nxt/graphics/rhi/core/device.hpp>

namespace nxt::graphics::vulkan {

/**
 * @brief rhi::Device의 Vulkan 구현이다.
 *
 * 물리 디바이스를 하나 골라 논리 디바이스를 만들고 queue를 얻는다. Phase 0은
 * queue family를 하나만 쓴다 — 그래픽스와 present가 같은 큐일 수 있다 (rhi.md §8).
 *
 * @note rhi::Instance::createDevice()가 이 클래스를 만들어
 *       unique_ptr<rhi::Device>로 돌려준다. 직접 만들지 않는다.
 * @note Thread safety: thread-safe하지 않다.
 */
class VulkanDevice final : public rhi::Device {
public:
    /**
     * @brief surface에 present할 수 있는 물리 디바이스와 queue family를 골라
     *        논리 디바이스를 만든다.
     *
     * @param[in] instance 물리 디바이스를 열거할 인스턴스.
     * @param[in] surface present 지원 여부를 확인할 대상.
     * @return 만들어진 디바이스.
     */
    [[nodiscard]]
    static std::unique_ptr<VulkanDevice> create(vk::Instance instance, vk::SurfaceKHR surface);

    /// @brief 논리 디바이스를 파괴한다. queue는 디바이스와 함께 사라진다.
    ~VulkanDevice() override;

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    /// @brief 커맨드 제출에 쓸 큐를 반환한다.
    [[nodiscard]]
    rhi::Queue graphicsQueue() const noexcept override;

    /// @brief present에 쓸 큐를 반환한다. Phase 0에서는 graphicsQueue()와 같을 수 있다.
    [[nodiscard]]
    rhi::Queue presentQueue() const noexcept override;

    /// @brief backend 구현부가 vk::Device를 얻는 통로이다 (fence · semaphore · memory).
    [[nodiscard]]
    vk::Device device() const noexcept {
        return device_;
    }

    /// @brief backend 구현부가 vk::PhysicalDevice를 얻는 통로이다 (memory type 선택).
    [[nodiscard]]
    vk::PhysicalDevice physicalDevice() const noexcept {
        return physicalDevice_;
    }

private:
    VulkanDevice(vk::PhysicalDevice physicalDevice, vk::Device device, vk::Queue graphicsQueue,
                 vk::Queue presentQueue) noexcept;

    /// @brief memory type과 surface 지원 조회에 쓴다.
    vk::PhysicalDevice physicalDevice_{};

    /// @brief 이 디바이스가 만든 리소스(fence · semaphore · memory)의 소유자이다.
    vk::Device device_{};

    /// @brief 커맨드 제출에 쓰는 큐.
    vk::Queue graphicsQueue_{};

    /// @brief present에 쓰는 큐.
    vk::Queue presentQueue_{};
};

} // namespace nxt::graphics::vulkan
