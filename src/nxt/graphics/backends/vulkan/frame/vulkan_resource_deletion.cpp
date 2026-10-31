#include <cstdint>
#include <functional>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/diagnostics/log.hpp>
#include <nxt/graphics/backends/vulkan/core/vulkan_device.hpp>
#include <nxt/graphics/rhi/core/device.hpp>
#include <nxt/graphics/rhi/frame/resource_deletion.hpp>
#include <utility>
#include <vector>

namespace nxt::graphics::vulkan {

namespace {

/**
 * @brief 지연 목록에 들어간 항목 하나이다.
 */
struct PendingDestroy {
    /// @brief 이 리소스를 마지막으로 사용한 프레임의 인덱스.
    std::uint32_t frameIndex = 0;

    /// @brief 파괴 시점에 부를 삭제자. 목록이 자기 디바이스를 넘겨 부른다.
    std::function<void(const rhi::Device&)> destroy;
};

} // namespace

/**
 * @brief rhi::ResourceDeletion의 Vulkan 구현이다.
 *
 * 실제 파괴는 전부 삭제자가 한다 — 이 클래스는 "언제 부를지"만 정한다. 그래서
 * vk:: 핸들도, Vulkan 호출도, 자원 종류에 대한 지식도 없다. 자원이 무엇인지
 * 모르는 채로 미룰 수 있는 것이 이 목록의 목적이다 (rhi.md §9).
 *
 * @note rhi::ResourceDeletion::create()만 이 클래스를 만든다. 다른 TU가 이름을 알
 *       필요가 없어 헤더를 두지 않는다 (VulkanCommandEncoder와 같은 이유).
 * @note 소유권: unique_ptr이 파괴한다. 삭제자가 캡처한 핸들은 이 객체보다 오래
 *       살아야 한다 — 목록이 파괴될 때 남은 항목의 삭제자를 부르기 때문이다.
 * @note Thread safety: thread-safe하지 않다. 프레임 루프와 같은 스레드에서 부른다.
 */
class VulkanResourceDeletion final : public rhi::ResourceDeletion {
public:
    /// @brief 빈 목록을 만든다. 삭제자를 부를 때 넘길 디바이스를 묶어 둔다.
    explicit VulkanResourceDeletion(const VulkanDevice& device) noexcept : device_(device) {}

    ~VulkanResourceDeletion() override {
        // 종료 경로다. GPU 사용이 끝났다는 보장은 호출자가 만든다. (resource_deletion.hpp)
        destroyAll();
    }

    VulkanResourceDeletion(const VulkanResourceDeletion&) = delete;
    VulkanResourceDeletion& operator=(const VulkanResourceDeletion&) = delete;

    void deferDestroy(std::uint32_t frameIndex, std::function<void(const rhi::Device&)> destroy) override {
        NXT_ASSERT_MSG(destroy != nullptr, "빈 삭제자를 지연 목록에 넣으려 했다.");
        pending_.push_back(PendingDestroy{frameIndex, std::move(destroy)});
    }

    void collect(std::uint32_t frameIndex) override {
        // 삭제자가 이 목록에 다시 넣을 수 있다 (image를 지우며 그 memory도 맡기는 경우).
        // 순회 중인 vector에 push_back 하면 반복자가 무효가 되므로 먼저 걷어내고 부른다.
        std::vector<PendingDestroy> due;
        for (PendingDestroy& entry : pending_) {
            if (entry.frameIndex == frameIndex) {
                due.push_back(std::move(entry));
            }
        }

        if (due.empty()) {
            return;
        }

        std::erase_if(pending_, [frameIndex](const PendingDestroy& entry) { return entry.frameIndex == frameIndex; });

        runDestroyers(due);
        NXT_LOG_DEBUG(Graphics, Vulkan, "resource deletion destroyed: count={} frame={}", due.size(), frameIndex);
    }

    void flush() override {
        destroyAll();
    }

private:
    /**
     * @brief 걷어낸 항목을 넣은 순서대로 부른다.
     *
     * 순서가 곧 의존성 순서다 — Pipeline을 Buffer보다 먼저 맡기면 파괴도 그 순서다
     * (frame.md §9). 정렬하지 않는 이유는 호출자가 이미 순서를 알고 있어서다.
     *
     * @param[in] due 부를 삭제자들. 비어 있어도 된다.
     */
    void runDestroyers(std::vector<PendingDestroy>& due) {
        for (PendingDestroy& entry : due) {
            entry.destroy(device_);
        }
    }

    /// @brief 프레임과 무관하게 남은 항목을 모두 지금 파괴한다.
    void destroyAll() {
        if (pending_.empty()) {
            return;
        }

        // swap으로 먼저 떼어 낸다. 삭제자가 다시 넣은 항목은 이번에 지우지 않고 다음
        // collect()로 미룬다 — 소멸자 경로에서는 다음 기회가 없으므로 남는다.
        std::vector<PendingDestroy> due;
        due.swap(pending_);

        runDestroyers(due);
        NXT_LOG_DEBUG(Graphics, Vulkan, "resource deletion flushed: count={}", due.size());
    }

    /// @brief 삭제자가 쓸 디바이스. 이 객체보다 오래 산다.
    const VulkanDevice& device_;

    /// @brief 아직 파괴하지 않은 항목. 들어온 순서를 유지한다.
    std::vector<PendingDestroy> pending_;
};

} // namespace nxt::graphics::vulkan

namespace nxt::graphics::rhi {

std::unique_ptr<ResourceDeletion> ResourceDeletion::create(const Device& device) {
    // make_unique는 private 생성자를 못 부른다. 여기서만 new를 쓴다 (VulkanSwapchain과 같은 이유).
    return std::unique_ptr<ResourceDeletion>(new vulkan::VulkanResourceDeletion(vulkan::toVulkanDevice(device)));
}

} // namespace nxt::graphics::rhi
