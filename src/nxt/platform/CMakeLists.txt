# src/nxt/platform/CMakeLists.txt

# OS API를 소유한다. GPU는 모른다.
# 서브디렉터리는 별도 CMakeLists 없이 이 파일 하나에서 등록한다.
# 백엔드만 예외로 backends/<os>/가 자체 CMakeLists를 가진다.

add_library(nxt_platform STATIC)

add_subdirectory(backends)

# Linux Wayland 백엔드는 wayland-client와 xdg-shell 프로토콜 생성물을 쓴다.
# 생성 파일은 소스가 아니라 빌드 디렉터리에 둔다. add_custom_command와 그 출력을
# 쓰는 타깃(nxt_platform)이 같은 디렉터리에 있어야 생성 규칙이 타깃에 붙는다.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

    # 배포판마다 경로가 다르므로 pkg-config가 알려 주는 pkgdatadir을 쓴다.
    pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

    set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml")
    set(WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated")
    set(XDG_SHELL_HEADER "${WAYLAND_GENERATED_DIR}/xdg-shell-client-protocol.h")
    set(XDG_SHELL_SOURCE "${WAYLAND_GENERATED_DIR}/xdg-shell-protocol.c")

    add_custom_command(
        OUTPUT "${XDG_SHELL_HEADER}" "${XDG_SHELL_SOURCE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${WAYLAND_GENERATED_DIR}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${XDG_SHELL_XML}" "${XDG_SHELL_HEADER}"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${XDG_SHELL_XML}" "${XDG_SHELL_SOURCE}"
        DEPENDS "${XDG_SHELL_XML}"
        COMMENT "Generating xdg-shell Wayland protocol"
        VERBATIM
    )

    # 생성 헤더를 include할 수 있게 하고, 생성 C 소스를 nxt_platform에 넣는다.
    target_sources(nxt_platform PRIVATE "${XDG_SHELL_SOURCE}" "${XDG_SHELL_HEADER}")
    target_include_directories(nxt_platform PRIVATE "${WAYLAND_GENERATED_DIR}")
    target_link_libraries(nxt_platform PRIVATE PkgConfig::WAYLAND_CLIENT)
endif()

target_sources(nxt_platform
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            window/window.hpp
            input/input_device.hpp
            filesystem/filesystem.hpp
)

# platform은 core에 의존한다. core가 platform을 알지는 않는다.
target_link_libraries(nxt_platform PUBLIC nxt_core)

target_include_directories(nxt_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(nxt_platform PRIVATE /W4)
else()
    target_compile_options(nxt_platform PRIVATE -Wall -Wextra)
endif()