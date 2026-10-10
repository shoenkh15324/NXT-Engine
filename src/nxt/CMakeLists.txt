# src/nxt/CMakeLists.txt

# 의존 방향은 아래와 같다. 역방향 의존을 만들지 않는다.
#
#   core, math            (의존 없음)
#     ↑
#   platform
#     ↑
#   graphics              ← backends/vulkan
#     ↑
#   renderer, engine, assets

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(platform)
add_subdirectory(graphics)
add_subdirectory(renderer)
add_subdirectory(engine)
add_subdirectory(assets)