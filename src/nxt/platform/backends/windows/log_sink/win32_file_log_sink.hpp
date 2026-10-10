#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <nxt/core/diagnostics/log.hpp>
#include <string>
#include <vector>

// 파일에 로그를 기록하는 Sink.
//
// Fatal은 즉시 flush하고, 그 외는 버퍼가 임계치에 찰 때만 파일로 넘긴다.

namespace nxt::platform::win32::log {

using namespace nxt::core::log;

/**
 * @brief 파일 Sink의 버퍼링 정책을 나타낸다.
 *
 * 버퍼 크기와 flush 임계치는 로그량과 파일 시스템 성능에 따라 달라지므로
 * 생성 시점에 주입받는다.
 */
struct FileLogBufferPolicy {
    /**
     * @brief 파일에 넘기기 전에 메모리에 쌓아 두는 바이트 수다.
     *
     * 이 크기를 넘는 한 줄은 버퍼를 거치지 않고 바로 파일로 쓴다.
     */
    std::size_t capacity{4096};

    /**
     * @brief 버퍼가 이 바이트 수에 도달하면 파일로 넘기고 비운다.
     *
     * capacity보다 클 수 없다. 0이면 capacity와 같다.
     */
    std::size_t flushThreshold{4096 * 80 / 100};

    /**
     * @brief flush 임계값이 capacity를 넘지 않도록 보정한다.
     */
    void normalize() noexcept {
        if (flushThreshold == 0 || flushThreshold > capacity) {
            flushThreshold = capacity;
        }
    }
};

/**
 * @brief 파일에 로그를 기록한다.
 *
 * 생성자는 경로와 버퍼 정책만 받아 OS에 대한 추가 의존을 만들지 않는다.
 * Fatal은 버퍼에 쌓인 내용과 함께 즉시 flush하고,
 * 그 외 레벨은 버퍼가 임계치에 찰 때만 파일에 기록한다.
 *
 * @note Thread safety: LogManager가 write() 호출을 직렬화하므로
 *       이 Sink 자체는 별도의 동기화를 하지 않는다.
 * @note 소멸 시 남은 버퍼와 파일 스트림 버퍼를 모두 비운다.
 *       비정상 종료로 끝나면 마지막 flush 이후의 기록은 유실된다.
 */
class Win32FileLogSink : public LogSink {
public:
    /**
     * @brief 기본 버퍼 정책으로 파일 로그 Sink를 생성한다.
     *
     * @param path 로그를 기록할 파일 경로.
     */
    explicit Win32FileLogSink(const std::filesystem::path& path);

    /**
     * @brief 지정한 버퍼 정책으로 파일 로그 Sink를 생성한다.
     *
     * 상위 디렉터리가 없으면 만든다. 파일을 열지 못해도 예외를 던지지 않으며
     * 이 경우 Sink는 아무것도 기록하지 않는다.
     *
     * @param path 로그를 기록할 파일 경로.
     * @param policy 버퍼 크기와 flush 임계값.
     */
    Win32FileLogSink(const std::filesystem::path& path, FileLogBufferPolicy policy);

    ~Win32FileLogSink() override;

    /**
     * @brief 로그 레코드를 파일 버퍼에 쌓는다.
     *
     * 버퍼가 임계치를 넘으면 파일에 기록하고 비운다.
     * Fatal이면 쌓인 내용과 이 레코드를 즉시 기록하고 flush한다.
     *
     * @param record 기록할 로그 레코드.
     */
    void write(const LogRecord& record) noexcept override;

    /**
     * @brief 로그 파일을 열었는지 여부를 반환한다.
     */
    [[nodiscard]]
    bool isOpen() const noexcept;

    /**
     * @brief 실행 파일 옆 logs 디렉터리의 로그 파일 경로를 반환한다.
     *
     * 파일명에는 프로그램 시작 시각이 밀리초까지 들어간다.
     * nxt_YYYYMMDD_HHMMSS_mmm.log 형태다.
     *
     * 시각이 들어가 보통 매 실행마다 새 파일이 생기므로 지난 로그가 남는다.
     * 같은 밀리초에 두 번 실행해 파일명이 겹치면 기존 내용을 지운다.
     *
     * 실행 파일 위치를 확인할 수 없으면 현재 디렉터리를 사용한다.
     *
     * @return 기본 로그 파일 경로.
     */
    [[nodiscard]]
    static std::filesystem::path defaultPath();

private:
    /**
     * @brief 버퍼에 쌓인 기록을 파일에 쓰고 버퍼를 비운다.
     *
     * 파일 스트림 버퍼는 비우지 않는다.
     */
    void flushBuffer() noexcept;

    /**
     * @brief 파일 스트림 버퍼까지 비워 디스크에 확정한다.
     */
    void commit() noexcept;

    /**
     * @brief 버퍼에 한 줄을 쌓고 필요하면 파일로 넘긴다.
     *
     * 남은 공간보다 길면 먼저 비운 뒤 넣는다.
     * 버퍼 전체보다 긴 줄은 버퍼를 거치지 않고 단독으로 쓴다.
     *
     * @param line 누적할 한 줄. 끝에 개행이 포함되어 있다.
     */
    void append(const std::string& line) noexcept;

    std::ofstream stream_;
    std::vector<char> buffer_; // 버퍼는 생성 시 capacity만큼만 할당하고 이후에는 늘리지 않는다.
    std::size_t flushThreshold_{0};
    std::size_t buffered_{0};
};

} // namespace nxt::platform::win32::log
