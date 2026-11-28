# tests/nxt/core/CMakeLists.txt

add_executable(nxt_core_tests
    main.cpp
    handle_test.cpp
    assert_test.cpp
    log_test.cpp
    timer_test.cpp
    time_test.cpp
    allocator_test.cpp
    arena_test.cpp
    pool_test.cpp
    thread_source_test.cpp
    job_counter_test.cpp
    spsc_queue_test.cpp
    worker_test.cpp
)

target_link_libraries(nxt_core_tests PRIVATE
    nxt_core
    doctest::doctest
)

add_test(NAME nxt_core_tests COMMAND nxt_core_tests)
