#pragma once

#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/graphics/backends/vulkan/vulkan_types.hpp>
#include <string>

namespace nxt::graphics::vulkan {

/**
 * @brief Vulkan 호출 실패를 기록한 뒤 종료한다.
 *
 * 예외를 던지지 않는다 (rhi.md §7, CODING_RULES.md §13). 종료 경로이므로 로깅
 * 경로에 의존하지 않고 reportAssertFailure가 stderr에 직접 기록한다
 * (CODING_RULES.md §18).
 *
 * @param[in] result     실패를 나타내는 Vulkan 결과.
 * @param[in] expression 실패한 호출식.
 * @param[in] function   실패가 난 함수 이름.
 * @param[in] file       실패가 난 파일.
 * @param[in] line       실패가 난 줄.
 */
[[noreturn]]
inline void reportVulkanFailure(vk::Result result, const char* expression, const char* function, const char* file,
                                int line) noexcept {
    const std::string resultName = vk::to_string(result);
    ::nxt::core::diag::reportAssertFailure({expression, resultName.c_str(), file, function, line});
}

} // namespace nxt::graphics::vulkan

/**
 * @brief Vulkan 호출 결과를 검사하고, 실패하면 종료한다.
 *
 * Release에서도 항상 평가된다. GPU 오류는 복구할 수 없으므로 NXT_ASSERT처럼
 * 사라지면 안 된다 (rhi.md §7).
 *
 * @note 값이 있는 호출은 결과를 먼저 받은 뒤 `.result`를 넘긴다. `ResultValue<T>`를
 *       그대로 넘기면 담긴 값을 버리게 되므로 받지 않는다.
 *
 * ```cpp
 * const auto created = vk::createInstance(createInfo);
 * VK_CHECK(created.result);
 * vk::Instance instance = created.value;
 * ```
 */
#define VK_CHECK(expression)                                                                                           \
    do {                                                                                                               \
        const ::vk::Result nxtVkResult = (expression);                                                                 \
        if (nxtVkResult != ::vk::Result::eSuccess) {                                                                   \
            ::nxt::graphics::vulkan::reportVulkanFailure(nxtVkResult, #expression, __func__, __FILE__, __LINE__);      \
        }                                                                                                              \
    } while (false)
