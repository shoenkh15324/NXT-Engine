#pragma once

#include <cstdio>
#include <nxt/core/diagnostics/log.hpp>
#include <string>

/*
    Windows 콘솔에 로그를 기록하는 Sink.
*/

namespace nxt::platform::windows::log {

using namespace nxt::log;

/**
 * @brief 콘솔 출력에 사용할 색상을 나타낸다.
 *
 * 색상은 로그 데이터가 아니라 표현 정책이므로 LogRecord에 담지 않는다.
 */
enum class LogColor : std::uint8_t {
    White = 0,
    Grey,
    Cyan,
    Yellow,
    Red,
    BrightRed,
};

class Win32ConsoleLogSink : public LogSink {
public:
    /**
     * @brief 콘솔 로그 Sink를 생성한다.
     *
     * 표준 출력과 표준 오류 핸들에 VT 시퀀스 처리를 켠다.
     * 켜지지 않은 핸들에는 색상을 붙이지 않는다.
     */
    Win32ConsoleLogSink() noexcept;

    /**
     * @brief 로그 레코드를 콘솔에 기록한다.
     *
     * @param record 출력할 로그 레코드.
     */
    void write(const LogRecord& record) noexcept override;

    /**
     * @brief 로그 레벨에 대응하는 색상을 반환한다.
     *
     * @param level 변환할 로그 레벨.
     *
     * @return 로그 레벨에 대응하는 색상.
     */
    [[nodiscard]]
    static LogColor colorOf(LogLevel level) noexcept;

    /**
     * @brief 표준 출력에 색상을 붙일 수 있는지 여부를 반환한다.
     */
    [[nodiscard]]
    bool colorSupportedOnStandardOutput() const noexcept;

    /**
     * @brief 표준 오류에 색상을 붙일 수 있는지 여부를 반환한다.
     */
    [[nodiscard]]
    bool colorSupportedOnStandardError() const noexcept;

    /**
     * @brief 로그 레코드를 콘솔 한 줄 문자열로 조합한다.
     *
     * 출력 대상과 색 지원 여부에 의존하지 않는 순수 함수다.
     * write()는 이 결과를 표준 스트림에 쓴다.
     *
     * @param record 조합할 로그 레코드.
     * @param useColor 색상 이스케이프 시퀀스를 붙일지 여부.
     *
     * @return 조합된 콘솔 한 줄.
     */
    [[nodiscard]]
    static std::string format(const LogRecord& record, bool useColor);

private:
    std::FILE* streamFor(const LogLevel level) const noexcept;

    bool colorOnStandardOutput_;
    bool colorOnStandardError_;
};

} // namespace nxt::platform::windows::log
