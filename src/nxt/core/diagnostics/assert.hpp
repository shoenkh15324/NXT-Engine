#pragma once

namespace nxt::core::diag {

/// @brief assertion 실패를 담는 정보 구조체이다.
struct AssertInfo {
    const char* expression;
    const char* message;
    const char* file;
    const char* function;
    int line;
};

/**
 * @brief Assertion 실패를 처리하는 함수 포인터 타입이다.
 *
 * @note [[noreturn]]이 아니다. 기본 핸들러는 종료하지만, 교체된 핸들러가 복귀해도
 *       reportAssertFailure()가 대신 종료시키므로 핸들러는 언제든 복귀할 수 있다.
 *       이 덕분에 테스트가 핸들러를 끼워 넣고 결과를 관찰할 수 있다.
 */
using AssertHandler = void (*)(const AssertInfo& info) noexcept;

/**
 * @brief 등록된 핸들러에게 실패를 알리고 복귀한다.
 *
 * 종료하지 않는다.
 */
void notifyAssertFailure(const AssertInfo& info) noexcept;

/**
 * @brief 등록된 핸들러에게 실패를 알린 뒤 종료한다.
 *
 * 절대 복귀하지 않는다. NXT_ASSERT 계열이 호출하는 쪽이다.
 */
[[noreturn]]
void reportAssertFailure(const AssertInfo& info) noexcept;

/**
 * @brief 핸들러를 교체한다.
 *
 * nullptr을 주면 기본 핸들러로 되돌린다.
 *
 * @note Thread safety: thread-safe하지 않다. 프로그램 시작 시 한 번만 호출해야 하며,
 *       핸들러 교체와 assertion 실패가 동시에 일어나지 않아야 한다.
 */
void setAssertHandler(AssertHandler handler) noexcept;

} // namespace nxt::core::diag

/**
 * @brief 프로그래머 오류와 불변식 위반을 검증한다.
 *
 * @note NXT_ASSERT는 Release에서 조건을 평가하지 않는다. 그래서 assert에만 쓰이는
 *       변수는 Release에서 미사용 경고가 난다. 반드시 평가해야 하는 조건은
 *       NXT_VERIFY를 쓴다.
 */
#if defined(NDEBUG)

    #define NXT_ASSERT(expression) ((void)0)
    #define NXT_ASSERT_MSG(expression, message) ((void)0, (void)0)
    #define NXT_VERIFY(expression) ((void)(expression))
    #define NXT_VERIFY_MSG(expression, message) ((void)(message), (void)(expression))

#else

    #define NXT_ASSERT(expression)                                                                                     \
        do {                                                                                                           \
            if (!(expression)) {                                                                                       \
                ::nxt::core::diag::reportAssertFailure({#expression, nullptr, __FILE__, __func__, __LINE__});          \
            }                                                                                                          \
        } while (false)

    #define NXT_ASSERT_MSG(expression, message)                                                                        \
        do {                                                                                                           \
            if (!(expression)) {                                                                                       \
                ::nxt::core::diag::reportAssertFailure({#expression, message, __FILE__, __func__, __LINE__});          \
            }                                                                                                          \
        } while (false)

    #define NXT_VERIFY(expression)                                                                                     \
        do {                                                                                                           \
            if (!(expression)) {                                                                                       \
                ::nxt::core::diag::reportAssertFailure({#expression, nullptr, __FILE__, __func__, __LINE__});          \
            }                                                                                                          \
        } while (false)

    #define NXT_VERIFY_MSG(expression, message)                                                                        \
        do {                                                                                                           \
            if (!(expression)) {                                                                                       \
                ::nxt::core::diag::reportAssertFailure({#expression, message, __FILE__, __func__, __LINE__});          \
            }                                                                                                          \
        } while (false)

#endif // NDEBUG
