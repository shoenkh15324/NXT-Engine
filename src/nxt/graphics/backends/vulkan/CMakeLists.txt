# src/nxt/graphics/backends/vulkan/CMakeLists.txt

# Vulkan을 아는 파일은 이 디렉터리 아래에만 존재한다.
# 다른 어떤 파일도 vulkan 헤더를 직접 include하지 않는다.
# rhi가 이 디렉터리를 include하는 것은 허용되지만 반대는 금지된다.
#
# include 경로와 Vulkan::Vulkan 링크는 상위 graphics/CMakeLists.txt가 이미
# 설정했다. 여기서는 구현 파일만 등록한다.

# Phase 0-A에서 아래 목록의 주석을 해제한다.
# target_sources(nxt_graphics
#     PRIVATE
#         vulkan_types.hpp
#         vulkan_check.hpp
#         vulkan_instance.cpp
#         vulkan_device.cpp
#         vulkan_memory.cpp
#         vulkan_swapchain.cpp
#         vulkan_command_buffer.cpp
#         vulkan_fence.cpp
#         vulkan_resource_deletion.cpp
#         vulkan_shader.cpp
#         vulkan_pipeline.cpp
#         vulkan_buffer.cpp
#         vulkan_image.cpp
# )