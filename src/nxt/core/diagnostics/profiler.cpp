#include <nxt/core/diagnostics/profiler.hpp>

namespace nxt::core::diag {

namespace {

/**
 * @brief 이 스레드에서 Scope가 쓸 Profiler이다.
 *
 * thread_local이므로 워커 스레드가 생겨도 서로 섞이지 않는다. Phase 1은 메인
 * 스레드만 계측한다.
 */
thread_local Profiler* gCurrentProfiler = nullptr;

} // namespace

bool Profiler::enabled() const noexcept {
    // relaxed로 충분하다. 켜고 끄는 값이 프레임 데이터와 함께 동기화될 필요가 없다.
    return enabled_.load(std::memory_order_relaxed);
}

void Profiler::setEnabled(bool enabled) noexcept {
    enabled_.store(enabled, std::memory_order_relaxed);
}

void Profiler::beginFrame() noexcept {
    count_ = 0;
    frameActive_ = enabled();
}

void Profiler::endFrame() noexcept {
    // 표본은 samples_에 남아 있다. 호출자가 프레임이 끝난 뒤에 읽는다.
    frameActive_ = false;
}

std::span<const Sample> Profiler::samples() const noexcept {
    return {samples_.data(), count_};
}

void Profiler::setCurrent(Profiler* profiler) noexcept {
    gCurrentProfiler = profiler;
}

Profiler* Profiler::current() noexcept {
    return gCurrentProfiler;
}

std::size_t Profiler::beginScope(const char* name) noexcept {
    if (!frameActive_ || (count_ >= kMaxSamplesPerFrame)) {
        // 넘치면 조용히 버린다. 계측이 프레임을 실패시키면 안 된다.
        return kNoScope;
    }

    samples_[count_] = Sample{.name = name, .ns = 0};
    return count_++;
}

void Profiler::endScope(std::size_t index, std::uint64_t ns) noexcept {
    if (index < count_) {
        samples_[index].ns = ns;
    }
}

Scope::Scope(const char* name) noexcept {
    Profiler* profiler = Profiler::current();
    if ((profiler == nullptr) || !profiler->enabled()) {
        return;
    }

    const std::size_t index = profiler->beginScope(name);
    if (index == Profiler::kNoScope) {
        return;
    }

    profiler_ = profiler;
    index_ = index;
    start_.emplace(time::now());
}

Scope::~Scope() {
    if ((profiler_ == nullptr) || !start_) {
        return;
    }

    profiler_->endScope(index_, (time::now() - *start_).ns());
}

} // namespace nxt::core::diag
