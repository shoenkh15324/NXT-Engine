#include <cstdio>
#include <cstdlib>
#include <nxt/core/diagnostics/assert.hpp>

namespace nxt::core::diag {

namespace {

void defaultAssertHandler(const AssertInfo& info) noexcept {
    std::fprintf(stderr,
                 "\n"
                 "========== NXT ASSERTION FAILED ==========\n"
                 "Expression : %s\n"
                 "File       : %s\n"
                 "Line       : %d\n"
                 "Function   : %s\n",
                 info.expression, info.file, info.line, info.function);

    if (info.message != nullptr) {
        std::fprintf(stderr, "Message    : %s\n", info.message);
    }

    std::fprintf(stderr, "===========================================\n");
    std::fflush(stderr);

    std::abort();
}

AssertHandler gAssertHandler = &defaultAssertHandler;

} // namespace

void notifyAssertFailure(const AssertInfo& info) noexcept {
    gAssertHandler(info);
}

[[noreturn]]
void reportAssertFailure(const AssertInfo& info) noexcept {
    notifyAssertFailure(info);
    // 기본 핸들러는 위에서 종료하지만, 교체된 핸들러가 복귀할 수 있으므로 여기서 끝낸다.
    std::abort();
}

void setAssertHandler(AssertHandler handler) noexcept {
    gAssertHandler = (handler != nullptr) ? handler : &defaultAssertHandler;
}

} // namespace nxt::core::diag
