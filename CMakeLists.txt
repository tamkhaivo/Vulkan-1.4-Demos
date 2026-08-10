cmake_minimum_required(VERSION 3.20)
project(Vulkan14LearningSpecification LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan package
find_package(Vulkan REQUIRED)

# Option to build all assignments or selectively
option(BUILD_ALL_ASSIGNMENTS "Build all 10 Vulkan 1.4 assignments" ON)

find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "C:/vcpkg/installed/x64-windows/tools/shaderc")

function(add_vulkan_assignment ASSIGNMENT_DIR ASSIGNMENT_NAME)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
        add_executable(${ASSIGNMENT_NAME} "src/main.cpp")
        target_include_directories(${ASSIGNMENT_NAME} PRIVATE 
            "include"
            "${CMAKE_SOURCE_DIR}/common/include"
        )
        target_link_libraries(${ASSIGNMENT_NAME} PRIVATE vulkan_common Vulkan::Vulkan)
        set_target_properties(${ASSIGNMENT_NAME} PROPERTIES
            CXX_STANDARD 17
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        if(GLSLC_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
            file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")
            foreach(SHADER ${SHADER_FILES})
                get_filename_component(FILE_NAME ${SHADER} NAME)
                set(SPV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
                add_custom_command(
                    OUTPUT ${SPV_FILE}
                    COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPV_FILE}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling ${FILE_NAME} to SPIR-V"
                )
                target_sources(${ASSIGNMENT_NAME} PRIVATE ${SPV_FILE})
            endforeach()
        endif()
    endif()
endfunction()

# Add Subdirectories for Assignments
add_subdirectory(common)
add_subdirectory(assignment01_hello_triangle)
add_subdirectory(assignment02_rotating_cube)
add_subdirectory(assignment03_textured_quad)
add_subdirectory(assignment04_push_constants_dynamic_uniforms)
add_subdirectory(assignment05_instanced_rendering)
add_subdirectory(assignment06_two_pass_dynamic_rendering_local_read)
add_subdirectory(assignment07_compute_particles_indirect_draw)
add_subdirectory(assignment08_deferred_shading_g_buffer)
add_subdirectory(assignment09_multithreaded_command_recording)
add_subdirectory(assignment10_buffer_device_address_streaming)
