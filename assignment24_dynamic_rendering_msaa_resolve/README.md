# Assignment 24 – Direct Dynamic Rendering Multisampled Resolves & MSAA

## Overview
Implement hardware Multi-Sample Anti-Aliasing (MSAA) with inline color and depth resolves directly on GPU tile memory using Vulkan 1.4 dynamic rendering.

## Key Concepts
- Multisampled dynamic rendering without legacy render passes (`VkRenderingAttachmentInfo`).
- Multisample color and depth/stencil image allocation (`VK_SAMPLE_COUNT_4_BIT` / `VK_SAMPLE_COUNT_8_BIT`).
- Tile-local multisample resolve via `VkRenderingAttachmentInfo.resolveMode` (`VK_RESOLVE_MODE_AVERAGE_BIT` / `VK_RESOLVE_MODE_MIN_BIT`).
- Inline resolves saving external memory bandwidth and eliminating separate resolve blit passes.

## Acceptance Criteria
- [x] Query and select maximum supported sample count for color and depth formats.
- [x] Create multisampled color and depth attachments matching swapchain resolution.
- [x] Configure `VkRenderingAttachmentInfo` specifying multisampled target and swapchain resolve target.
- [x] Configure graphics pipeline rasterization state with matching sample counts.
- [x] Render 3D geometry observing crisp, anti-aliased edges and smooth depth transitions.

## Directory Structure
- `src/main.cpp`: MSAA dynamic rendering host application.
- `shaders/`: GLSL shaders for multisampled geometry rendering.
- `CMakeLists.txt`: Build target configuration.
