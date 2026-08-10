find_package(glfw3 CONFIG REQUIRED)

add_library(vulkan_common INTERFACE)

target_include_directories(vulkan_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vulkan_common INTERFACE Vulkan::Vulkan glfw)

