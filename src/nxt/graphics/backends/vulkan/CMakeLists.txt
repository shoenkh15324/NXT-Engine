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

# surface 생성만 플랫폼마다 다르다. 그래서 vulkan_instance만 두 파일로 나눈다.
# #if로 한 파일에 몰아넣지 않고 CMake가 구현 파일을 고르는 이유는, 플랫폼별 헤더
# (windows.h · wayland-client.h)가 해당 구현 파일 안에만 들어오게 하기 위해서다.
if(WIN32)
    set(NXT_VULKAN_INSTANCE_PLATFORM_SOURCE core/vulkan_instance_win32.cpp)

    # vk::Win32SurfaceCreateInfoKHR와 createWin32SurfaceKHR를 노출하려면 필요하다.
    # vulkan.hpp가 이 매크로가 있으면 windows.h를 include하므로 NOMINMAX와
    # WIN32_LEAN_AND_MEAN을 함께 건다 — 특정 파일이 아니라 타깃 전체에 필요하다.
    target_compile_definitions(nxt_graphics PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX AND NOT APPLE)
    set(NXT_VULKAN_INSTANCE_PLATFORM_SOURCE core/vulkan_instance_linux.cpp)

    # NativeWindow의 display가 wl_display*, handle이 wl_surface*다 (window.hpp).
    # vulkan.hpp가 VK_USE_PLATFORM_WAYLAND_KHR 때문에 wayland-client.h를 include하므로
    # 헤더 경로와 링크를 이 타깃에도 걸어 준다. platform의 PRIVATE 링크는 전파되지 않는다.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

    target_compile_definitions(nxt_graphics PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
    target_link_libraries(nxt_graphics PRIVATE PkgConfig::WAYLAND_CLIENT)
else()
    message(FATAL_ERROR "nxt_graphics: this platform has no Vulkan surface backend.")
endif()

target_sources(nxt_graphics
    # 백엔드 헤더. 이름이 HEADERS가 아니므로 TYPE을 명시한다.
    PRIVATE
        FILE_SET backend_headers
        TYPE HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            common/vulkan_types.hpp
            common/vulkan_check.hpp
            core/vulkan_instance.hpp
            core/vulkan_device.hpp
            frame/vulkan_swapchain.hpp

    PRIVATE
        core/vulkan_instance.cpp
        ${NXT_VULKAN_INSTANCE_PLATFORM_SOURCE}
        core/vulkan_device.cpp
        resource/vulkan_memory.cpp
        resource/vulkan_shader.cpp
        resource/vulkan_pipeline.cpp
        resource/vulkan_buffer.cpp
        resource/vulkan_image.cpp
        frame/vulkan_swapchain.cpp
        frame/vulkan_command_buffer.cpp
        frame/vulkan_fence.cpp
        frame/vulkan_query.cpp
        frame/vulkan_semaphore.cpp
        frame/vulkan_resource_deletion.cpp
)
