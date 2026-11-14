add_executable(sandbox main.cpp)

# nxt_core를 직접 링크한다. main.cpp가 nxt/core를 include하므로
# nxt_platform의 public 전이에 의존하지 않는다.
target_link_libraries(sandbox PRIVATE
    nxt_core
    nxt_platform
    nxt_graphics
    nxt_renderer
    nxt_engine
)

# GLSL을 SPIR-V로 컴파일한다.
# nxt_add_shader는 CMAKE_CURRENT_SOURCE_DIR 기준으로 소스를 찾으므로,
# 이 디렉터리에서 루트 shaders/까지의 상대 경로를 넘긴다.
nxt_add_shaders(NXT_TRIANGLE_SPV
    ../../shaders/triangle.vert
    ../../shaders/triangle.frag
)

# 생성된 .spv를 타깃 소스로 등록한다.
# .spv는 C++ 컴파일 대상이 아니라 custom command의 출력이며,
# 이 줄이 있어야 빌드 전에 SPIR-V 생성 규칙이 실행된다.
target_sources(sandbox PRIVATE ${NXT_TRIANGLE_SPV})

# 실행 파일 옆 shaders/ 디렉토리로 복사한다.
# 작업 디렉토리가 아니라 실행 파일 위치를 기준으로 하므로
# Visual Studio에서 실행해도 경로가 맞는다.
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sandbox>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NXT_TRIANGLE_SPV} "$<TARGET_FILE_DIR:sandbox>/shaders"
    COMMAND_EXPAND_LISTS
)

# 시작 배너에 표시할 빌드 정보를 컴파일 시점에 상수로 넘긴다.
target_compile_definitions(sandbox PRIVATE
    NXT_VERSION_STRING="${PROJECT_VERSION}"
    NXT_PLATFORM_NAME="Windows x64"
    $<$<CONFIG:Debug>:NXT_BUILD_TYPE="Debug">
    $<$<CONFIG:Release>:NXT_BUILD_TYPE="Release">
    $<$<CONFIG:RelWithDebInfo>:NXT_BUILD_TYPE="RelWithDebInfo">
    $<$<CONFIG:MinSizeRel>:NXT_BUILD_TYPE="MinSizeRel">

    # 셰이더 로딩 경로. 실행 파일 위치 기준 절대 경로이다.
    NXT_SHADER_DIR="$<TARGET_FILE_DIR:sandbox>/shaders"
)
