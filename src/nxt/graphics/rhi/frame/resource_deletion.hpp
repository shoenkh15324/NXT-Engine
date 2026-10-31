#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace nxt::graphics::rhi {

class Device;

/**
 * @brief GPU가 아직 쓰고 있을 수 있는 리소스를 안전한 시점까지 붙잡아 두는 목록이다.
 *
 * 트리플 버퍼링에서는 GPU가 여러 프레임을 동시에 처리한다. 지금 파괴하면 그중 하나가
 * 아직 접근하므로 즉시 파괴는 use-after-free다 (frame.md §9).
 *
 * deferDestroy()는 리소스를 마지막으로 사용한 프레임 인덱스와 함께 삭제자를 목록에
 * 넣고, collect()가 그 프레임의 fence가 신호된 뒤에 삭제자를 부른다. "언제 파괴해도
 * 되는가"는 프레임을 정의하는 이 모듈이 알고, 호출자는 순서만 지키면 된다 (rhi.md §9).
 *
 * @code
 * deletion->deferDestroy(frameIndex, [memory](const Device& device) { freeMemory(device, memory); });
 * @endcode
 *
 * @note 값 핸들이 아니라 추상 클래스인 이유는 목록과 디바이스를 들고 있기 때문이다
 *       (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권: create()가 만들고 unique_ptr이 파괴한다. 파괴 시 남은 항목도 함께
 *       파괴하므로 그 전에 GPU 사용이 끝나 있어야 한다.
 * @note Thread safety: thread-safe하지 않다. 프레임 루프와 같은 스레드에서 부른다.
 */
class ResourceDeletion {
public:
    /**
     * @brief 빈 지연 목록을 만든다.
     *
     * @param[in] device 이 목록이 묶일 디바이스. 목록보다 오래 산다.
     * @return 만들어진 목록.
     */
    [[nodiscard]]
    static std::unique_ptr<ResourceDeletion> create(const Device& device);

    /// @brief 남은 항목을 모두 파괴한다. GPU 사용이 끝난 뒤여야 한다.
    virtual ~ResourceDeletion() = default;

    ResourceDeletion(const ResourceDeletion&) = delete;
    ResourceDeletion& operator=(const ResourceDeletion&) = delete;

    /**
     * @brief 리소스를 마지막으로 사용한 프레임 인덱스와 함께 목록에 넣는다.
     *
     * 지금 파괴하지 않는다. 그 인덱스의 collect()가 삭제자를 부를 때 실제로 파괴된다.
     *
     * @param[in] frameIndex 이 리소스를 마지막으로 사용한 프레임의 인덱스.
     * @param[in] destroy    실제 파괴를 수행하는 삭제자. freeMemory() 같은 기존 해제
     *                       함수를 감싼다. 목록이 자기 디바이스를 넘겨 부른다.
     *
     * @note 삭제자는 목록이 부를 때까지 유효해야 한다. 핸들은 값으로 캡처하면 된다 —
     *       디바이스는 목록이 넘긴다.
     * @note 같은 프레임에 여러 개를 넣으면 넣은 순서대로 파괴된다. Pipeline을
     *       Buffer보다 먼저 넣어야 한다 (frame.md §9).
     */
    virtual void deferDestroy(std::uint32_t frameIndex, std::function<void(const Device&)> destroy) = 0;

    /**
     * @brief 그 프레임이 마지막으로 쓴 리소스를 지금 파괴한다.
     *
     * @param[in] frameIndex 파괴할 항목의 프레임 인덱스.
     *
     * @note 그 프레임의 fence를 기다린 뒤에만 부른다. 그 대기가 "이 리소스는 더 이상
     *       쓰이지 않는다"의 유일한 근거다 (frame.md §9). 순서를 바꾸면 파괴 직전에
     *       GPU가 여전히 쓰는 리소스를 지운다.
     * @note 프레임 루프는 재사용할 프레임 인덱스의 fence를 기다린 직후 그 인덱스로
     *       부른다 (frame.md §9 처리 시점).
     */
    virtual void collect(std::uint32_t frameIndex) = 0;

    /**
     * @brief 목록의 모든 항목을 프레임과 무관하게 지금 파괴한다.
     *
     * 리사이즈와 종료 경로가 쓴다. 리사이즈에서는 모든 in-flight fence를 기다린 뒤에
     * 부른다 (frame.md §6). 파괴를 별도 경로로 만들지 않고 같은 목록을 비우는 이유는
     * 경로가 둘이면 한쪽만 순서를 지키게 되기 때문이다.
     */
    virtual void flush() = 0;

protected:
    /**
     * @brief backend 파생 클래스만 만들 수 있다.
     *
     * 복사 생성자를 지웠으므로 기본 생성자도 함께 사라진다. 파생 클래스가 기반을
     * 만들 수 있도록 여기서 되살린다.
     */
    ResourceDeletion() = default;
};

} // namespace nxt::graphics::rhi
