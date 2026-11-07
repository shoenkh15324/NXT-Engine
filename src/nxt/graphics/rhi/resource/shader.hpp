#pragma once

#include <cstdint>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <span>

namespace nxt::graphics::rhi {

class Device;

/// @brief Shader를 다른 종류의 리소스와 구분하기 위한 빈 태그이다.
struct ShaderTag {};

/**
 * @brief SPIR-V로 컴파일된 셰이더 모듈이다.
 *
 * 파이프라인을 만들 때 단계별로 하나씩 넘긴다. 모듈 자체는 어떤 단계인지 알지
 * 못한다 — vertex인지 fragment인지는 파이프라인 기술자가 정한다 (rhi.md §8).
 *
 * @note 값 핸들이다. vk 핸들 외에 자체 상태가 없고, 배열로 들고 다닐 이유도
 *       없으므로 추상 클래스로 만들지 않는다 (phase0.md "RHI 타입은 두 패턴").
 * @note 소유권이 없다. createShader()가 만들고 destroyShader()가 해제한다.
 *       파이프라인 생성이 끝나면 바로 해제할 수 있다 — 버퍼나 이미지처럼 GPU가
 *       실행 중에 참조하는 리소스가 아니다.
 */
using Shader = ResourceHandle<ShaderTag>;

/**
 * @brief SPIR-V 코드로 셰이더 모듈을 만든다.
 *
 * Vulkan은 이 호출에서 코드를 읽어 모듈을 만들므로, 반환 뒤 code 버퍼를
 * 유지할 필요가 없다.
 *
 * @note 파일 읽기는 RHI의 책임이 아니다. 호출자가 SPIR-V 파일을 읽어 워드를
 *       넘긴다. Phase 2에서 platform/filesystem이 그 책임을 맡는다.
 *
 * @param[in] device 셰이더 모듈을 만들 디바이스.
 * @param[in] code SPIR-V 워드 배열. 비어 있으면 안 된다. std::span<const
 *                 std::uint32_t>이므로 4바이트 정렬이 보장된다.
 *
 * @return 만들어진 셰이더 모듈.
 */
[[nodiscard]]
Shader createShader(const Device& device, std::span<const std::uint32_t> code);

/**
 * @brief 셰이더 모듈을 해제한다.
 *
 * 이 모듈로 파이프라인을 만든 뒤에 호출한다. 파이프라인은 모듈이 만든 코드를
 * 이미 소비했으므로, 해제를 fence 신호까지 미룰 필요가 없다 — buffer/image와
 * 다른 점이다 (rhi.md §9).
 *
 * @param[in] device 모듈을 만든 디바이스.
 * @param[in] shader 해제할 셰이더 모듈.
 */
void destroyShader(const Device& device, const Shader& shader);

} // namespace nxt::graphics::rhi
