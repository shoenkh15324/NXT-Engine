# tests/nxt/platform/CMakeLists.txt

# 플랫폼 테스트는 OS별 구현을 직접 부르므로, 그 OS에서만 소스를 넣는다.
# Linux Wayland 창 테스트는 compositor가 필요하므로 아직 넣지 않는다.
if(WIN32)
    set(NXT_PLATFORM_TEST_SOURCES
        win32_console_log_sink_test.cpp
        win32_file_log_sink_test.cpp
    )
elseif(UNIX AND NOT APPLE)
    set(NXT_PLATFORM_TEST_SOURCES
        posix_console_log_sink_test.cpp
        posix_file_log_sink_test.cpp
    )
else()
    set(NXT_PLATFORM_TEST_SOURCES)
endif()

add_executable(nxt_platform_tests
    main.cpp
    ${NXT_PLATFORM_TEST_SOURCES}
)

target_link_libraries(nxt_platform_tests PRIVATE
    nxt_platform
    doctest::doctest
)

add_test(NAME nxt_platform_tests COMMAND nxt_platform_tests)
