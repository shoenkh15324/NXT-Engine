# src/nxt/platform/CMakeLists.txt

# OS API를 소유한다. GPU는 모른다.
# 서브디렉터리는 별도 CMakeLists 없이 이 파일 하나에서 등록한다.
# 백엔드만 예외로 backends/<os>/가 자체 CMakeLists를 가진다.

add_library(nxt_platform STATIC)

add_subdirectory(backends)

target_sources(nxt_platform
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            window/window.hpp
            input/input_device.hpp
            filesystem/filesystem.hpp
)

# platform은 core에 의존한다. core가 platform을 알지는 않는다.
target_link_libraries(nxt_platform PUBLIC nxt_core)

target_include_directories(nxt_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(nxt_platform PRIVATE /W4)
else()
    target_compile_options(nxt_platform PRIVATE -Wall -Wextra)
endif()