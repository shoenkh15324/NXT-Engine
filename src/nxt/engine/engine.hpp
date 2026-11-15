#pragma once

#include <cstdint>
#include <memory>
#include <nxt/engine/frame_loop.hpp>
#include <nxt/graphics/rhi/rhi.hpp>
#include <nxt/platform/window/window.hpp>
#include <nxt/renderer/renderer.hpp>
#include <span>
#include <string_view>

namespace nxt::engine {

/**
 * @brief RHI 타입을 짧게 쓰기 위한 별칭이다.
 *
 * 헤더에서 using namespace를 전개하면 이 헤더를 include하는 모든 TU에 rhi의 이름이
 * 풀린다. 공개 계약인 헤더에서는 별칭 하나만 노출하고 타입은 rhi::로 한정한다.
 */
namespace rhi = ::nxt::graphics::rhi;

/**
 * @brief Engine을 만들 때 필요한 설정이다.
 *
 * 모든 필드에 기본값이 있으므로 앱은 자기가 바꾸는 값만 지정한다. 셰이더 배열은
 * create()가 끝날 때까지만 유효하면 된다 — 파이프라인을 만드는 동안만 읽는다.
 *
 * @note title · appName은 create() 호출 중 유효하면 충분하다. 백엔드와 instance가
 *       생성 시 복사한다.
 */
struct EngineDesc {
    /// @brief GPU 드라이버에 알릴 앱 이름이다. InstanceDesc::appName이 된다.
    std::string_view appName{"NXT Engine"};

    /// @brief 창 설정이다. 제목과 처음 크기를 정한다.
    platform::window::WindowDesc window{};

    /// @brief 버텍스 단계 SPIR-V 워드 배열이다.
    std::span<const std::uint32_t> vertexShader;

    /// @brief 프래그먼트 단계 SPIR-V 워드 배열이다.
    std::span<const std::uint32_t> fragmentShader;
};

/**
 * @brief 서브시스템을 조립하고 소유하며, 프레임을 순회한다.
 *
 * 창 → instance → surface → device → swapchain → renderer → frame loop 순서로 만들고
 * 정확히 그 역순으로 파괴한다 (frame.md §9). acquire · record · submit · present 중
 * "언제"를 정하는 것이 이 클래스의 책임이다 (frame.md §2). 무엇을 그리는지는
 * renderer가, 어떻게 제출하는지는 nxt_graphics가 안다.
 *
 * 앱(main)은 로깅을 세우고, 셰이더를 읽어 EngineDesc에 넣고, run()을 부른다. 앱이
 * GPU 핸들이나 동기화 객체를 직접 만질 일은 없다.
 *
 * @note create()로만 만든다. 창 생성만 복구 가능한 실패라 create()가 nullptr로
 *       알리고, 나머지 조립은 실패하면 assert로 멈춘다 (rhi의 다른 create와 같다).
 * @note Thread safety: thread-safe하지 않다. 주 스레드에서만 쓴다. 창 이벤트와
 *       프레임 기록이 같은 스레드에 있어야 한다.
 */
class Engine final {
public:
    /**
     * @brief 창과 GPU 서브시스템을 만들고 프레임 루프를 준비한다.
     *
     * @param[in] desc 창 · 앱 이름 · 셰이더 설정.
     * @return 조립된 엔진. 창을 만들지 못하면 nullptr이고 원인은 로그에 남는다.
     */
    [[nodiscard]]
    static std::unique_ptr<Engine> create(const EngineDesc& desc);

    /**
     * @brief 프레임 루프를 먼저 버리고 나머지를 역순으로 파괴한다.
     *
     * 프레임 루프의 소멸자가 fence 대기와 waitIdle을 마치므로, 그 뒤에 렌더러의
     * 버퍼를 버리는 순서가 보장된다 (frame.md §9).
     */
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /**
     * @brief 창이 닫힐 때까지 프레임을 반복한다.
     *
     * 프레임마다 이벤트를 처리하고, 최소화 상태면 쉬고, 창 크기와 swapchain 크기가
     * 다르면 재생성하고, 프레임 하나를 제출 · 제시한다. swapchain이 낡았다는
     * 결과를 받으면 그 자리에서 재생성하고 다음 프레임을 이어간다.
     *
     * @note 1초마다 fps를 로그로 남긴다.
     */
    void run();

    /**
     * @brief 씬을 채울 때 쓰는 렌더러를 반환한다.
     *
     * 앱은 이 객체로 메시를 올린다. Phase 2에서 World가 생기면 앱은 World만 만지고
     * 이 접근자는 사라진다.
     */
    [[nodiscard]]
    renderer::Renderer& renderer() noexcept;

private:
    /**
     * @brief 이미 만든 창을 넘겨받아 나머지 서브시스템을 조립한다.
     *
     * 창 생성만 복구 가능한 실패이므로 create()가 그것을 처리하고, 여기서는
     * 실패할 수 없는 조립만 한다.
     *
     * @param[in] desc   셰이더와 앱 이름. 생성 중에만 읽는다.
     * @param[in] window create()가 만든 창. 이 객체가 소유한다.
     */
    Engine(const EngineDesc& desc, std::unique_ptr<platform::window::Window> window);

    /**
     * @brief 프레임 하나를 실행한다.
     *
     * @return 계속할 수 있으면 true. 창이 닫혔으면 false이다.
     */
    [[nodiscard]]
    bool pumpFrame();

    /// @brief 창을 소유한다. surface가 이 창의 핸들을 참조하므로 가장 나중에 파괴한다.
    std::unique_ptr<platform::window::Window> window_;

    /// @brief Vulkan instance를 소유한다. surface도 이 객체가 파괴한다.
    std::unique_ptr<rhi::Instance> instance_;

    /// @brief 창에 대응하는 surface이다. 값 핸들이며 instance가 파괴한다.
    rhi::Surface surface_{};

    /// @brief GPU 디바이스를 소유한다. 렌더러와 프레임 루프보다 오래 산다.
    std::unique_ptr<rhi::Device> device_;

    /// @brief swapchain을 소유한다. 프레임 루프가 이 객체를 빌려 쓴다.
    std::unique_ptr<rhi::Swapchain> swapchain_;

    /// @brief 씬을 기록하는 렌더러를 소유한다.
    std::unique_ptr<renderer::Renderer> renderer_;

    /// @brief 프레임 루프를 소유한다. 렌더러를 참조로 잡으므로 렌더러보다 먼저 파괴한다.
    std::unique_ptr<FrameLoop> frameLoop_;
};

} // namespace nxt::engine
