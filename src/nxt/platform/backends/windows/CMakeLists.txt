# src/nxt/platform/backends/windows/CMakeLists.txt

# Windows 전용 구현은 이 타깃에 등록한다.

target_sources(nxt_platform
    PRIVATE
        log_sink/win32_console_log_sink.cpp
        log_sink/win32_file_log_sink.cpp
        win32_event_pump.cpp
        win32_input.cpp
        win32_window.cpp
)

target_sources(nxt_platform
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            log_sink/win32_console_log_sink.hpp
            log_sink/win32_file_log_sink.hpp
            win32_event_pump.hpp
            win32_input.hpp
            win32_window.hpp
)
