#pragma once

#include <nxt/core/jobs/thread_source.hpp>

namespace nxt::core::jobs {

/**
 * @brief std::thread로 스레드를 만드는 ThreadSource이다.
 *
 * 상태가 없으므로 start() 자체는 어느 스레드에서 불러도 안전하다. 다만 사용 계약은
 * ThreadSource를 따른다 — 스케줄러 스레드 하나가 부른다.
 *
 * @note 스레드 생성 실패(std::system_error)는 복구하지 않는다. 워커 풀을 세우는
 *       프로그램 시작 단계의 실패이므로 호출자에게 그대로 전달된다. core는 try/catch를
 *       쓰지 않는다(CODING_RULES §13).
 */
class StdThreadSource final : public ThreadSource {
public:
    [[nodiscard]]
    std::unique_ptr<Thread> start(ThreadFunction body) override;
};

} // namespace nxt::core::jobs
