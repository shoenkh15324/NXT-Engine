# src/nxt/graphics/backends/vulkan/CMakeLists.txt

# Vulkan을 아는 파일은 이 디렉터리 아래에만 존재한다.
# 다른 어떤 파일도 vulkan 헤더를 직접 include하지 않는다.
#
# include 경로와 Vulkan::Vulkan 링크는 상위 graphics/CMakeLists.txt가 설정했다.
# 여기서는 구현 파일만 등록한다.
#
# vulkan_types.hpp는 backend 구현부만 include한다. 그래서 공개 FILE_SET이# 아니라 PRIVATE FILE_SET에 둔다.

target_sources(nxt_graphics
    # 백엔드 헤더. 이름이 HEADERS가 아니므로 TYPE을 명시한다.
    PRIVATE
        FILE_SET backend_headers
        TYPE HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            vulkan_types.hpp
            vulkan_check.hpp

    PRIVATE
        vulkan_instance.cpp
        vulkan_device.cpp
        vulkan_memory.cpp
        vulkan_swapchain.cpp
        vulkan_command_buffer.cpp
        vulkan_fence.cpp
        vulkan_resource_deletion.cpp
)
