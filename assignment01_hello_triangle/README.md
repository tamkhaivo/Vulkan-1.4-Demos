# Assignment 1 – Hello Triangle (Dynamic Rendering)

## Overview
Set up a complete Vulkan application that clears the screen to a solid colour and draws a single triangle without using a render pass object.

## Key Concepts
- `VkInstance`, physical device selection, logical device creation with validation layers enabled.
- Swapchain setup (`VkSwapchainKHR`) with color image views.
- Dynamic rendering using `vkCmdBeginRendering` and `VkRenderingInfo`.
- Pipeline creation using `VkPipelineRenderingCreateInfo`.
- Per-frame fence synchronization.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL / SPIR-V shaders (`triangle.vert`, `triangle.frag`).
- `CMakeLists.txt`: Build target configuration.
