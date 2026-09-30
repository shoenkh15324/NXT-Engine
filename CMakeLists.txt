cmake_minimum_required(VERSION 3.25)

project(
    NXT
    VERSION 0.1.0
    LANGUAGES CXX
)

option(NXT_BUILD_SANDBOX "Build NXT sandbox application" ON)
option(NXT_BUILD_TESTS "Build NXT tests" OFF)

add_subdirectory(src/nxt)

if(NXT_BUILD_SANDBOX)
    add_subdirectory(apps/sandbox)
endif()

if(NXT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
