# src/nxt/platform/CMakeLists.txt

add_library(nxt_platform STATIC
    filesystem/filesystem.hpp
    input/input_device.hpp
    window/window.hpp
)

add_subdirectory(backends)

# platform은 core에 의존한다. core가 platform을 알지는 않는다.
target_link_libraries(nxt_platform PUBLIC nxt_core)

target_include_directories(nxt_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(nxt_platform PRIVATE /W4)
else()
    target_compile_options(nxt_platform PRIVATE -Wall -Wextra)
endif()