# Assignment 6 – Two-Pass Effect with Dynamic Rendering Local Reads

## Overview
Implement an on-chip multipass post-processing effect (scene rendering followed by edge detection or gaussian blur) utilizing Vulkan 1.4 core Dynamic Rendering Local Reads (`VK_KHR_dynamic_rendering_local_read`) to sample intermediate attachments without legacy subpass dependencies or render pass objects.

## Key Concepts
- Vulkan 1.4 Core Dynamic Rendering Local Reads (`dynamicRenderingLocalRead` feature).
- `VkRenderingInputAttachmentIndexInfoKHR` and `VkRenderingAttachmentLocationInfoKHR` chained in dynamic rendering info.
- Shader `subpassInput` declarations and `subpassLoad()` queries in GLSL without `VkRenderPass`.
- Memory dependency hazard resolution with `VK_DEPENDENCY_BY_REGION_BIT` and `VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT` -> `VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT`.
- Zero tile-memory spill and eliminated framebuffer allocation overhead.

## Acceptance Criteria
- [x] Enable `dynamicRenderingLocalRead` feature in `VkPhysicalDeviceFeatures2`.
- [x] Create an intermediate offscreen color attachment image formatted for both color attachment and input attachment usage.
- [x] Execute Pass 1: Render 3D animated geometry into the offscreen color target within `vkCmdBeginRendering`.
- [x] Insert regional execution and memory barrier via `vkCmdPipelineBarrier2` with `VK_DEPENDENCY_BY_REGION_BIT`.
- [x] Execute Pass 2: Bind local read post-processing pipeline and draw fullscreen quad, reading offscreen pixel data via `subpassLoad()`.
- [x] Resolve final filtered color to swapchain output with zero validation errors.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`scene.vert`, `scene.frag`, `post.vert`, `post.frag`).
- `CMakeLists.txt`: Build target configuration.
