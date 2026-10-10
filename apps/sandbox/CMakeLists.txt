add_executable(sandbox main.cpp)

# nxt_core를 직접 링크한다. main.cpp가 nxt/core를 include하므로
# nxt_platform의 public 전이에 의존하지 않는다.
target_link_libraries(sandbox PRIVATE nxt_core nxt_platform)

# 시작 배너에 표시할 빌드 정보를 컴파일 시점에 상수로 넘긴다.
target_compile_definitions(sandbox PRIVATE
    NXT_VERSION_STRING="${PROJECT_VERSION}"
    NXT_PLATFORM_NAME="Windows x64"
    $<$<CONFIG:Debug>:NXT_BUILD_TYPE="Debug">
    $<$<CONFIG:Release>:NXT_BUILD_TYPE="Release">
    $<$<CONFIG:RelWithDebInfo>:NXT_BUILD_TYPE="RelWithDebInfo">
    $<$<CONFIG:MinSizeRel>:NXT_BUILD_TYPE="MinSizeRel">
)
