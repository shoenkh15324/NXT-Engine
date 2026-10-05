# tests/nxt/platform/CMakeLists.txt

add_executable(nxt_platform_tests
    main.cpp
    win32_console_log_sink_test.cpp
    win32_file_log_sink_test.cpp
)

target_link_libraries(nxt_platform_tests PRIVATE
    nxt_platform
    doctest::doctest
)

add_test(NAME nxt_platform_tests COMMAND nxt_platform_tests)