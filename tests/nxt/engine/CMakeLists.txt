# tests/nxt/engine/CMakeLists.txt

add_executable(nxt_engine_tests
    main.cpp
    frame_stats_test.cpp
)

target_link_libraries(nxt_engine_tests PRIVATE
    nxt_engine
    doctest::doctest
)

add_test(NAME nxt_engine_tests COMMAND nxt_engine_tests)
