# src/nxt/platform/backends/CMakeLists.txt

# 각 백엔드는 이 위치에서 추가한다.
# 아직 구현되지 않은 백엔드는 디렉터리만 만들어 두고 등록하지 않는다.

if(WIN32)
    add_subdirectory(windows)
endif()