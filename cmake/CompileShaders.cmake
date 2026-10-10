# cmake/CompileShaders.cmake

# GLSL을 SPIR-V로 컴파일한다. 컴파일러는 find_package(Vulkan)이 준다.
#
# 사용법:
#   nxt_add_shader(VAR_NAME shaders/triangle.vert)
#   target_sources(... PRIVATE ${VAR_NAME})
#
# 출력은 ${CMAKE_CURRENT_BINARY_DIR} 아래에 .spv로 생성된다.
# 소스 헤더의 include 경로는 -I로 넘기지 않는다. NXT의 셰이더는 상대 경로
# include를 쓰지 않는다.

if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR
        "glslangValidator not found. find_package(Vulkan) did not provide it.")
endif()

function(nxt_add_shader out_var source)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
        message(FATAL_ERROR "shader not found: ${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    endif()

    get_filename_component(shader_name "${source}" NAME_WE)
    set(spv "${CMAKE_CURRENT_BINARY_DIR}/${shader_name}.spv")

    add_custom_command(
        OUTPUT "${spv}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}"
                -V "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
                -o "${spv}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMENT "Compiling shader ${shader_name}"
        VERBATIM
    )

    set(${out_var} "${spv}" PARENT_SCOPE)
endfunction()

# 여러 셰이더를 한 번에 등록한다.
#   nxt_add_shaders(VAR_NAME shaders/a.vert shaders/b.frag)
function(nxt_add_shaders out_var)
    set(outputs "")
    foreach(shader IN LISTS ARGN)
        nxt_add_shader(single "${shader}")
        list(APPEND outputs "${single}")
    endforeach()
    set(${out_var} "${outputs}" PARENT_SCOPE)
endfunction()