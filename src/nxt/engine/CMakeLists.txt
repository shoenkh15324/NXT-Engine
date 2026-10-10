# src/nxt/engine/CMakeLists.txt

# 서브시스템을 조립하고 프레임 수서를 정한다.
# acquire → record → submit → present 중 "언제"를 호출할지만 결정한다.
# GPU 드라이버 내부와 RHI 세부사항은 모른다.

add_library(nxt_engine STATIC
    engine.cpp
    frame_loop.cpp
)

target_sources(nxt_engine
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            engine.hpp
            frame_loop.hpp
)

target_include_directories(nxt_engine PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(nxt_engine
    PUBLIC nxt_core
    PUBLIC nxt_platform
    PUBLIC nxt_graphics
    PUBLIC nxt_renderer
)

if(MSVC)
    target_compile_options(nxt_engine PRIVATE /W4)
else()
    target_compile_options(nxt_engine PRIVATE -Wall -Wextra)
endif()

# Phase 0-C에서 scene 그래프가 시작되면 그때 추가한다.
#     scene/scene.hpp