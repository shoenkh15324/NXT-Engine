# src/nxt/assets/CMakeLists.txt

# 애셋을 디스크에서 읽고 CPU 측 데이터로 만드는 계층이다.
# GPU 리소스를 만들지 않는다. 그 경계는 nxt_renderer/world가 가진다.
# 따라서 nxt_graphics에 의존하지 않는다.

add_library(nxt_assets STATIC
    asset_manager.cpp
)

target_sources(nxt_assets
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            asset_manager.hpp
            asset_registry.hpp
)

target_include_directories(nxt_assets PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(nxt_assets
    PUBLIC nxt_core
    PUBLIC nxt_platform
)

if(MSVC)
    target_compile_options(nxt_assets PRIVATE /W4)
else()
    target_compile_options(nxt_assets PRIVATE -Wall -Wextra)
endif()

# Phase 2에서 로더가 생기면 아래에 추가한다.
#     types/asset_type.hpp
#     loaders/asset_loader.hpp
#     loaders/mesh_loader.hpp
#     loaders/texture_loader.hpp