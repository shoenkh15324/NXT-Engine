# tests/nxt/core/CMakeLists.txt

add_executable(nxt_core_tests
    main.cpp
    handle_test.cpp
    assert_test.cpp
    log_test.cpp
    timer_test.cpp
    time_test.cpp
)

target_link_libraries(nxt_core_tests PRIVATE
    nxt_core
    doctest::doctest
)

add_test(NAME nxt_core_tests COMMAND nxt_core_tests)
