# src/nxt/platform/backends/CMakeLists.txt

# 지원 플랫폼을 고르는 곳이다.
# OS를 추가하면 if 블록을 하나 더 쓰고 그 백엔드 파일을 나열한다.
#
# 여기서는 nxt_platform에 소스만 추가한다. 타깃 정의는 platform/CMakeLists.txt가 한다.

if(WIN32)
    target_sources(nxt_platform
        PRIVATE
            windows/log_sink/win32_console_log_sink.cpp
            windows/log_sink/win32_file_log_sink.cpp
            windows/win32_event_pump.cpp
            windows/win32_input.cpp
            windows/win32_window.cpp
    )

    # 이 백엔드의 헤더는 공개 계약이 아니므로 별도 FILE_SET으로 등록한다.
    # 공개 헤더는 platform/CMakeLists.txt의 FILE_SET HEADERS에 있다.
    # BASE_DIRS가 src이므로 여기서는 windows/ 경로가 앞에 붙는다.
    target_sources(nxt_platform
        PRIVATE
            FILE_SET backend_headers TYPE HEADERS
            BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
            FILES
                windows/log_sink/win32_console_log_sink.hpp
                windows/log_sink/win32_file_log_sink.hpp
                windows/win32_event_pump.hpp
                windows/win32_input.hpp
                windows/win32_window.hpp
    )
endif()
