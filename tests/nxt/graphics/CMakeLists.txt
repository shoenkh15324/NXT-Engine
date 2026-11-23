# tests/nxt/graphics/CMakeLists.txt

add_executable(nxt_graphics_tests
    main.cpp
    enums_test.cpp
    handle_test.cpp
)

target_link_libraries(nxt_graphics_tests PRIVATE
    nxt_math
    doctest::doctest
)

add_test(NAME nxt_graphics_tests COMMAND nxt_graphics_tests)
