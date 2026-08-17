# Assignment 10 – Buffer Device Address and Zero-Copy Streaming

## Overview
Stream dynamic vertex and transformation data directly from the CPU into ReBAR host-visible / device-local GPU memory using raw 64-bit buffer GPU pointers (`VK_KHR_buffer_device_address` / Vulkan 1.4 core BDA), dereferencing pointers directly inside GLSL shaders via `GL_EXT_buffer_reference2` without descriptor sets or staging buffers.

## Key Concepts
- Buffer Device Address (`bufferDeviceAddress` feature in Vulkan 1.2+ / 1.4 core) and `vkGetBufferDeviceAddress`.
- Shader raw pointer dereferencing via `GL_EXT_buffer_reference` / `GL_EXT_buffer_reference2`.
- Zero-copy ReBAR host-visible + device-local memory allocation (`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`).
- 64-bit GPU pointers passed via lightweight Push Constants.
- Ring-buffer dynamic streaming with fence synchronization to prevent CPU-GPU write-after-read race conditions.

## Acceptance Criteria
- [x] Enable `bufferDeviceAddress` feature in Vulkan physical and logical device creation.
- [x] Create buffers with `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` and query 64-bit addresses via `vkGetBufferDeviceAddress`.
- [x] Implement GLSL vertex and fragment shaders using `buffer_reference` struct blocks to dereference raw GPU addresses.
- [x] Pass 64-bit `VkDeviceAddress` values to the shader using `vkCmdPushConstants`.
- [x] Continuously stream animated vertex deformation data into mapped memory ring buffers each frame.
- [x] Render procedural animated geometry seamlessly with zero descriptor sets allocated for vertex/uniform streams.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`bda.vert`, `bda.frag`).
- `CMakeLists.txt`: Build target configuration.
