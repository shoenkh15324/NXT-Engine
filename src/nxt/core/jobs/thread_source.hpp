#pragma once

#include <functional>
#include <memory>

namespace nxt::core::jobs {

/**
 * @brief 새 스레드에서 실행할 함수이다.
 *
 * 스레드를 만들 때 한 번 옮겨지고, body가 끝날 때까지 유효해야 한다. 캡처한 객체의
 * 수명은 호출자가 보장한다 — body가 실행되는 동안 그 객체가 살아 있어야 한다.
 */
using ThreadFunction = std::function<void()>;

/**
 * @brief 만들어진 스레드 하나를 나타낸다.
 *
 * 소유권은 start()를 호출한 쪽에 있다. 반환된 포인터를 파괴하기 전에 join()을 정확히
 * 한 번 불러야 한다. join()은 body가 끝날 때까지 블록한다.
 *
 * @note join() 없이 파괴하면 std 구현은 std::thread의 규칙대로 terminate한다. 이는
 *       수명 계약 위반을 조용히 넘기지 않기 위해서다.
 * @note Thread safety: join()은 소유자 스레드 하나만 부른다.
 */
class Thread {
public:
    Thread() = default;
    virtual ~Thread() = default;

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

    /// @brief body가 끝날 때까지 기다린다. 스레드마다 정확히 한 번만 부른다.
    virtual void join() = 0;
};

/**
 * @brief 스레드 생성과 종료를 분리하는 인터페이스이다.
 *
 * Worker · WorkerPool(1-B4 · 1-B5)이 std::thread 대신 이 인터페이스를 잡으면, 테스트는
 * 스레드를 만들지 않는 fake source로 같은 로직을 검증할 수 있다.
 *
 * @note 소유권: start()가 만든 Thread의 소유권은 호출자에게 넘어간다.
 * @note Thread safety: thread-safe하지 않다. 스케줄러를 소유한 스레드 하나만 부른다.
 */
class ThreadSource {
public:
    ThreadSource() = default;
    virtual ~ThreadSource() = default;

    ThreadSource(const ThreadSource&) = delete;
    ThreadSource& operator=(const ThreadSource&) = delete;
    ThreadSource(ThreadSource&&) = delete;
    ThreadSource& operator=(ThreadSource&&) = delete;

    /**
     * @brief 새 스레드를 만들고 body를 그 안에서 실행한다.
     *
     * @param[in] body 새 스레드에서 실행할 함수. 비어 있으면 안 된다.
     * @return 만들어진 스레드. 소유자는 호출자이며, 파괴 전에 join()을 불러야 한다.
     */
    [[nodiscard]]
    virtual std::unique_ptr<Thread> start(ThreadFunction body) = 0;
};

} // namespace nxt::core::jobs
