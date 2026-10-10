# src/nxt/math/CMakeLists.txt

# 순수 계산 헤더만 있는 모듈이다. 링크 대상이 없으므로 INTERFACE로 둔다.
# 의존성이 없으므로 어떤 모듈보다도 먼저 올 수 있다.

add_library(nxt_math INTERFACE)

target_include_directories(nxt_math INTERFACE "${PROJECT_SOURCE_DIR}/src")

# 헤더를 추가할 때 아래 FILE_SET에 등록한다.
# target_sources(nxt_math INTERFACE
#     FILE_SET HEADERS
#     BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
#     FILES
#         math/vec3.hpp
#         math/mat4.hpp
# )