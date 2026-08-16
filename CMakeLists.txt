cmake_minimum_required(VERSION 3.20)
project(Vulkan14LearningSpecification LANGUAGES C CXX)

# Enforce C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Check and enforce Clang compiler standards
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Configured with Clang compiler (${CMAKE_CXX_COMPILER_VERSION}) targeting Vulkan 1.4 Standard")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
        message(WARNING "Clang version ${CMAKE_CXX_COMPILER_VERSION} is less than recommended Clang 17.0+")
    endif()
endif()

# Find Vulkan package (Vulkan 1.4)
find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan SDK Version: ${Vulkan_VERSION}")

# Option to build all assignments
option(BUILD_ALL_ASSIGNMENTS "Build all 10 Vulkan 1.4 assignments" ON)

# Find glslc shader compiler for SPIR-V generation targeting Vulkan 1.4
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "C:/vcpkg/installed/x64-windows/tools/shaderc")

# Target setup function for Vulkan 1.4 assignments
function(add_vulkan_assignment ASSIGNMENT_DIR ASSIGNMENT_NAME)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
        add_executable(${ASSIGNMENT_NAME} "src/main.cpp")
        
        target_include_directories(${ASSIGNMENT_NAME} PRIVATE 
            "include"
            "${CMAKE_SOURCE_DIR}/common/include"
        )
        
        target_link_libraries(${ASSIGNMENT_NAME} PRIVATE vulkan_common Vulkan::Vulkan)
        
        # Configure Clang-specific flags and compiler definitions
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            if(MSVC)
                target_compile_options(${ASSIGNMENT_NAME} PRIVATE /W4 -Wno-unused-parameter -Wno-unused-variable)
            else()
                target_compile_options(${ASSIGNMENT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
            endif()
        endif()
        
        set_target_properties(${ASSIGNMENT_NAME} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )

        # Shader compilation step targeting Vulkan 1.4 SPIR-V
        if(GLSLC_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
            file(GLOB SHADER_FILES 
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" 
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag" 
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.mesh"
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.task"
            )
            foreach(SHADER ${SHADER_FILES})
                get_filename_component(FILE_NAME ${SHADER} NAME)
                set(SPV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
                add_custom_command(
                    OUTPUT ${SPV_FILE}
                    COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 ${SHADER} -o ${SPV_FILE}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling ${FILE_NAME} to Vulkan 1.4 SPIR-V"
                )
                target_sources(${ASSIGNMENT_NAME} PRIVATE ${SPV_FILE})
            endforeach()
        endif()
    endif()
endfunction()

# Add Subdirectories for Common & All Assignments
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
add_subdirectory(assignment11_mesh_task_shading)
add_subdirectory(assignment12_descriptor_buffers)
add_subdirectory(assignment13_pipeline_binaries_cache)
add_subdirectory(assignment14_subgroup_arithmetic_reduction)
add_subdirectory(assignment15_ray_queries_inline)
add_subdirectory(assignment16_gpu_driven_draw_indirect_count)
add_subdirectory(assignment17_shader_objects)
add_subdirectory(assignment18_hardware_ray_tracing_pipeline)
add_subdirectory(assignment19_variable_rate_shading)
add_subdirectory(assignment20_sparse_virtual_texturing)
add_subdirectory(assignment21_bindless_texturing)
add_subdirectory(assignment22_async_compute_transfer_overlap)
add_subdirectory(assignment23_conditional_rendering_occlusion_queries)
add_subdirectory(assignment24_dynamic_rendering_msaa_resolve)
add_subdirectory(assignment25_clustered_forward_lighting)
add_subdirectory(assignment26_device_generated_commands)
add_subdirectory(assignment27_extended_dynamic_state3)
add_subdirectory(assignment28_calibrated_timestamps_gpu_profiling)
add_subdirectory(assignment29_host_image_copy)
add_subdirectory(assignment30_mesh_shading_culling_lod)
