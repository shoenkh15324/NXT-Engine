# src/nxt/core/CMakeLists.txt

add_library(nxt_core STATIC
    diagnostics/log.cpp
)

target_sources(nxt_core
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            handle/handle.hpp
            handle/handle_manager.hpp
)

target_include_directories(nxt_core PUBLIC "${PROJECT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(nxt_core PRIVATE /W4)
else()
    target_compile_options(nxt_core PRIVATE -Wall -Wextra)
endif()
