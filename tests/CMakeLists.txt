# tests/CMakeLists.txt

# 설치된 CMake가 4.x라서 cmake_minimum_required(VERSION 3.0)인 doctest를 거부한다.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/releases/download/v2.5.3/doctest-v2.5.3.tar.gz
    URL_HASH SHA256=eb4a39729d091e43d4d593888bf4d145ee9a0f0e13f7404484b6f2fb60d5d50f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(doctest)

add_subdirectory(nxt/core)
add_subdirectory(nxt/platform)
