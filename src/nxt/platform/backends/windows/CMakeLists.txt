# src/nxt/platform/backends/windows/CMakeLists.txt

# Windows 전용 구현을 nxt_platform에 등록한다.
# 이 디렉터리의 헤더는 공개 계약에 넣지 않는다. 공개 API는
# platform/window, platform/input, platform/filesystem 가 노출한다.

target_sources(nxt_platform
    PRIVATE
        log_sink/win32_console_log_sink.cpp
        log_sink/win32_file_log_sink.cpp
        win32_event_pump.cpp
        win32_input.cpp
        win32_window.cpp
)

# 백엔드 전용 헤더는 공개 계약이 아니므로 별도의 FILE_SET으로 등록한다.
# 공개 헤더는 platform/CMakeLists.txt의 FILE_SET HEADERS에 있다.
target_sources(nxt_platform
    PRIVATE
        FILE_SET backend_headers TYPE HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            log_sink/win32_console_log_sink.hpp
            log_sink/win32_file_log_sink.hpp
            win32_event_pump.hpp
            win32_input.hpp
            win32_window.hpp
)