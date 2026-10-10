# src/nxt/renderer/CMakeLists.txt

# 렌더링 씬을 소유한다. GPU를 다루는 방법은 모르고 nxt_graphics에 위임한다.
# Vulkan과 Win32을 직접 include하지 않는다.

add_library(nxt_renderer STATIC
    renderer.cpp
)

target_sources(nxt_renderer
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            renderer.hpp
            mesh/mesh.hpp
            world/world.hpp
)

target_include_directories(nxt_renderer PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(nxt_renderer
    PUBLIC nxt_core
    PUBLIC nxt_math
    PUBLIC nxt_graphics
)

# Phase 0-B / 0-C에서 파일을 만들면 아래에 추가한다.
#     frame_context.hpp
#     passes/triangle_pass.hpp
#
# Phase 2 / 3에서 material·graph가 생기면 그때 추가한다.
#     material/material.hpp
#     graph/render_graph.hpp

if(MSVC)
    target_compile_options(nxt_renderer PRIVATE /W4)
else()
    target_compile_options(nxt_renderer PRIVATE -Wall -Wextra)
endif()