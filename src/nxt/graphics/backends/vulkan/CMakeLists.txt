# src/nxt/graphics/backends/vulkan/CMakeLists.txt

# Vulkan을 아는 파일은 이 디렉터리 아래에만 존재한다.
# 다른 어떤 파일도 vulkan 헤더를 직접 include하지 않는다.
#
# include 경로와 Vulkan::Vulkan 링크는 상위 graphics/CMakeLists.txt가 설정했다.
# 여기서는 구현 파일만 등록한다.
#
# 파일이 늘어나므로 rhi/와 같은 4분류로 나눈다. 새 모듈은 이 중 하나에 들어간다.
#   common/    vk:: 경계 — vulkan_types.hpp 하나만 vulkan.hpp를 include한다 · VK_CHECK
#   core/      프로그램 수명 — instance(플랫폼 분기 포함) · device
#   frame/     프레임 실행과 동기 — swapchain · command_buffer · fence · semaphore ·
#              resource_deletion
#   resource/  GPU 리소스 — memory · buffer · image · shader · pipeline
#
# vulkan_types.hpp는 backend 구현부만 include한다. 그래서 공개 FILE_SET이 아니라 PRIVATE FILE_SET에 둔다.

target_sources(nxt_graphics
    # 백엔드 헤더. 이름이 HEADERS가 아니므로 TYPE을 명시한다.
    PRIVATE
        FILE_SET backend_headers
        TYPE HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            common/vulkan_types.hpp
            common/vulkan_check.hpp

    PRIVATE
        core/vulkan_instance.cpp
        core/vulkan_device.cpp
        resource/vulkan_memory.cpp
        frame/vulkan_swapchain.cpp
        frame/vulkan_command_buffer.cpp
        frame/vulkan_fence.cpp
        frame/vulkan_semaphore.cpp
        frame/vulkan_resource_deletion.cpp
)
