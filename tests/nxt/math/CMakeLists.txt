# tests/nxt/math/CMakeLists.txt

add_executable(nxt_math_tests
    main.cpp
    vec3_test.cpp
    mat4_test.cpp
    aabb_test.cpp
)

target_link_libraries(nxt_math_tests PRIVATE
    nxt_math
    doctest::doctest
)

add_test(NAME nxt_math_tests COMMAND nxt_math_tests)
