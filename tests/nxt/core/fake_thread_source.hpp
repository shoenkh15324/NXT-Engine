#pragma once

#include <cstddef>
#include <memory>
#include <nxt/core/diagnostics/assert.hpp>
#include <nxt/core/jobs/thread_source.hpp>
#include <utility>
#include <vector>

namespace nxt::test {

/**
 * @brief 실제 스레드를 만들지 않는 Thread이다. join() 호출만 기록한다.
 *
 * @note 소유권: FakeThreadSource가 만들고 테스트가 unique_ptr로 소유한다. 들고 있는
 *       join 횟수 포인터는 source의 멤버를 가리키므로 source보다 오래 살 수 없다.
 */
class FakeThread final : public core::jobs::Thread {
public:
    explicit FakeThread(std::size_t& joinCount) noexcept : joinCount_(&joinCount) {}

    void join() override {
        joined_ = true;
        ++(*joinCount_);
    }

    /// @brief join()이 불렸는지 여부이다.
    [[nodiscard]]
    bool joined() const noexcept {
        return joined_;
    }

private:
    /// @brief 소유자인 source의 join 횟수이다.
    std::size_t* joinCount_;

    /// @brief join()이 불렸는지 여부이다.
    bool joined_{false};
};

/**
 * @brief body를 저장만 하고 실행하지 않는 ThreadSource이다.
 *
 * 생성·종료 순서와 body 전달을 스레드 없이 검증한다. 저장한 body는 테스트가
 * body(index)로 꺼내 직접 실행한다.
 *
 * @note 소유권: body는 source가 소유한다. 반환한 Thread도 source의 멤버를 참조하므로
 *       source보다 오래 살 수 없다.
 */
class FakeThreadSource final : public core::jobs::ThreadSource {
public:
    [[nodiscard]]
    std::unique_ptr<core::jobs::Thread> start(core::jobs::ThreadFunction body) override {
        bodies_.push_back(std::move(body));
        return std::make_unique<FakeThread>(joinCount_);
    }

    /// @brief 지금까지 start()가 받은 body 수이다.
    [[nodiscard]]
    std::size_t startCount() const noexcept {
        return bodies_.size();
    }

    /// @brief 지금까지 join()이 불린 횟수이다.
    [[nodiscard]]
    std::size_t joinCount() const noexcept {
        return joinCount_;
    }

    /// @brief index번째 body이다. 테스트가 스레드 대신 직접 실행한다.
    [[nodiscard]]
    core::jobs::ThreadFunction& body(std::size_t index) noexcept {
        NXT_ASSERT(index < bodies_.size());
        return bodies_[index];
    }

private:
    /// @brief start()가 받은 body이다. 테스트가 실행할 때까지 보관한다.
    std::vector<core::jobs::ThreadFunction> bodies_;

    /// @brief join()이 불린 횟수이다.
    std::size_t joinCount_{0};
};

} // namespace nxt::test
