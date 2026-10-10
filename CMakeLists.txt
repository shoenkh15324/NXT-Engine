# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.25)

project(
    NXT
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 소스는 UTF-8 BOM 없이 저장한다. MSVC의 기본 소스 코드 페이지는 시스템 ACP이므로
# 한국어 주석이 있는 상태에서는 이 플래그가 없으면 C4819가 발생한다.
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(NXT_BUILD_SANDBOX "Build NXT sandbox application" ON)
option(NXT_BUILD_TESTS "Build NXT tests" OFF)

# graphics 계층이 Vulkan에 의존하므로 configure 시점에 반드시 필요하다.
# find_package가 include 경로, 로더 라이브러리, glslang 경로를 모두 준다.
find_package(Vulkan REQUIRED)

# Vulkan::Vulkan은 include 디렉터리를 SYSTEM으로 표시하지 않는다.
# 각 모듈이 자기 쪽에서 SYSTEM으로 지정한다.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompileShaders)

add_subdirectory(src/nxt)

if(NXT_BUILD_SANDBOX)
    add_subdirectory(apps/sandbox)
endif()

if(NXT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
