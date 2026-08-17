# Assignment 41 – Multi-View Stereo & Foveated VR Rendering (`VK_KHR_multiview` / Vulkan 1.4 Core)

## Overview
Accelerate VR and stereoscopic rendering by broadcasting a single draw call into multiple framebuffer layers (Left & Right eyes) with distinct projection matrices using hardware Multi-View rasterization.

## Key Concepts
- `VkPhysicalDeviceMultiviewFeatures.multiview` feature configuration.
- `VkRenderingInfo` multi-view dynamic rendering with `viewMask`.
- Accessing `gl_ViewIndex` inside vertex, task, and mesh shaders to index per-eye transformation arrays.
- Eliminating CPU command submission overhead and redundant vertex processing for stereoscopic viewports.
- Integrating multi-view dynamic rendering with depth and color layer arrays (`VkImageViewType` `VK_IMAGE_VIEW_TYPE_2D_ARRAY`).

## Acceptance Criteria
- [x] Enable `multiview` feature and configure a dynamic rendering pass with `viewMask = 0b11` (Eyes 0 & 1).
- [x] Create a 2D array texture with 2 layers for left/right eye targets.
- [x] Write a vertex / mesh shader that reads `gl_ViewIndex` to fetch eye-specific MVP matrices from a UBO/BDA struct.
- [x] Submit a single draw pass that renders both eye perspectives concurrently in one submission.
- [x] Render side-by-side or split-screen stereoscopic visuals with 50% CPU draw overhead reduction.

## Directory Structure
- `src/main.cpp`: Multi-view dynamic rendering setup, layer array textures, and stereoscopic draw host application.
- `CMakeLists.txt`: Build target configuration.
