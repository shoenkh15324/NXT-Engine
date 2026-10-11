# src/nxt/graphics/CMakeLists.txt

# GPU를 다루는 계층이다. 씬 · 머티리얼 · 렌더 패스는 모른다.
# Vulkan을 아는 파일은 backends/vulkan/ 아래에만 존재해야 한다.
add_library(nxt_graphics STATIC)

add_subdirectory(backends/vulkan)

# core의 로깅·assert를 쓴다. platform은 창 핸들을 넘겨야 한다
# (rhi.md §8 — Instance가 VkSurfaceKHR을 만든다).
target_link_libraries(nxt_graphics
    PUBLIC nxt_core
    PUBLIC nxt_platform
)

target_include_directories(nxt_graphics PUBLIC "${PROJECT_SOURCE_DIR}/src")

# Vulkan::Vulkan은 include 디렉터리를 SYSTEM으로 표시하지 않는다.
# 그대로 쓰면 Vulkan 헤더의 경고가 /W4 빌드를 오염시킨다.
#
# PRIVATE이어야 한다. PUBLIC이나 INTERFACE면 이 경로가 nxt_renderer와
# nxt_engine까지 전파되어 RHI 밖에서 vulkan.hpp를 include할 수 있게 되고
# RHI 경계가 무너진다 (rhi.md §5 원칙 3).
#
# 그래도 vulkan-1.lib은 최종 링크에 남는다. STATIC 라이브러리의 PRIVATE
# 의존성은 LINK_ONLY로 소비자에게 전달되기 때문이다.
target_include_directories(nxt_graphics SYSTEM PRIVATE "${Vulkan_INCLUDE_DIR}")
target_link_libraries(nxt_graphics PRIVATE Vulkan::Vulkan)

# Vulkan-Hpp의 동적 디스패처를 쓴다. vulkan_types.hpp 안에서 #define하면
# include 순서에 따라 TU마다 값이 갈리므로 타깃에서 준다.
target_compile_definitions(nxt_graphics PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

if(MSVC)
    target_compile_options(nxt_graphics PRIVATE /W4)
else()
    target_compile_options(nxt_graphics PRIVATE -Wall -Wextra)
endif()

# 공개 헤더. Vk 타입이 새지 않는지 확인할 수 있도록 PUBLIC으로 내보낸다.
# 아직 없는 파일은 적지 않는다. 없는 파일을 적으면 configure가 실패한다.
target_sources(nxt_graphics
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            rhi/rhi.hpp
            rhi/semaphore.hpp
)
