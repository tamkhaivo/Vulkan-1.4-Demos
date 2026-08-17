# Assignment 1 – Hello Triangle (Dynamic Rendering)

## Overview
Set up a complete Vulkan 1.4 application that clears the swapchain image to a solid color and renders a single animated RGB triangle using modern Vulkan 1.4 core Dynamic Rendering without legacy `VkRenderPass` or `VkFramebuffer` objects.

## Key Concepts
- Vulkan 1.4 Instance and Physical/Logical Device setup with validation layers.
- Swapchain creation (`VkSwapchainKHR`) and image view initialization.
- Vulkan 1.4 Dynamic Rendering using `vkCmdBeginRendering` and `VkRenderingInfo`.
- Graphics Pipeline configuration using `VkPipelineRenderingCreateInfo`.
- Vulkan 1.4 Synchronization2 image layout transitions (`vkCmdPipelineBarrier2` / `VkImageMemoryBarrier2`).
- Per-frame Fence (`VkFence`) and Semaphore (`VkSemaphore`) synchronization.

## Acceptance Criteria
- [x] Initialize Vulkan 1.4 Instance and Logical Device with dynamic rendering enabled.
- [x] Create GLFW window surface and establish a swapchain (`VkSwapchainKHR`) with color attachment image views.
- [x] Compile and load vertex and fragment SPIR-V shaders (`triangle.vert`, `triangle.frag`).
- [x] Create graphics pipeline targeting color attachment formats via `VkPipelineRenderingCreateInfo` with `renderPass = VK_NULL_HANDLE`.
- [x] Record command buffer executing `vkCmdBeginRendering` and `vkCmdEndRendering` with `VK_ATTACHMENT_LOAD_OP_CLEAR` and `VK_ATTACHMENT_STORE_OP_STORE`.
- [x] Manage swapchain presentation lifecycle with `VkSemaphore` and `VkFence` frame pacing.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL / SPIR-V shaders (`triangle.vert`, `triangle.frag`).
- `CMakeLists.txt`: Build target configuration.
