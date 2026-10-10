# src/nxt/graphics/CMakeLists.txt

# GPU를 다루는 계층이다. 씬 · 머티리얼 · 렌더 패스는 모른다.
# Vulkan을 아는 파일은 backends/vulkan/ 아래에만 존재해야 한다.

# Phase 0-A에서 vulkan_*.cpp가 추가되면 STATIC으로 전환한다.
# 지금은 소스가 하나도 없어서 INTERFACE로 선언한다.
add_library(nxt_graphics INTERFACE)

add_subdirectory(backends/vulkan)

target_include_directories(nxt_graphics INTERFACE "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(nxt_graphics INTERFACE Vulkan::Vulkan)

# Vulkan::Vulkan은 include 디렉터리를 SYSTEM으로 표시하지 않는다.
# 그대로 쓰면 Vulkan 헤더의 경고가 /W4 빌드를 오염시키므로 여기서 SYSTEM으로 지정한다.
# 경로는 find_package가 이미 해석한 변수를 쓴다.
target_include_directories(nxt_graphics SYSTEM INTERFACE "${Vulkan_INCLUDE_DIR}")

# 헤더를 추가할 때 아래 FILE_SET에 등록한다.
# Vk 타입이 새지 않는지 확인할 수 있도록 PUBLIC으로 내보낸다.
# target_sources(nxt_graphics
#     PUBLIC
#         FILE_SET HEADERS
#         BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
#         FILES
#             rhi/rhi.hpp
#             rhi/enums.hpp
#             rhi/memory.hpp
#             rhi/instance.hpp
#             rhi/device.hpp
#             rhi/swapchain.hpp
#             rhi/command_buffer.hpp
#             rhi/fence.hpp
#             rhi/resource_deletion.hpp
# )