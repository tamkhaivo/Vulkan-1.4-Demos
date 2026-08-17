# Assignment 8 – Deferred Shading with Multiple Render Targets (MRT)

## Overview
Implement a deferred rendering pipeline using Multiple Render Targets (MRT) with Vulkan 1.4 core Dynamic Rendering Local Reads. Render Position, Normal, Albedo, and Depth into separate G-Buffer attachments in Pass 1, then execute a deferred lighting pass in Pass 2 reading all G-Buffer attachments on-chip via `subpassLoad()` to evaluate dynamic point lights.

## Key Concepts
- Multiple Color Attachments in Dynamic Rendering (`VkPipelineRenderingCreateInfo::pColorAttachmentFormats`).
- G-Buffer layout design:
  - Position: `VK_FORMAT_R16G16B16A16_SFLOAT`
  - Normal: `VK_FORMAT_R16G16B16A16_SFLOAT`
  - Albedo/Roughness: `VK_FORMAT_R8G8B8A8_UNORM`
  - Depth: `VK_FORMAT_D32_SFLOAT`
- Dynamic Rendering Local Reads (`VK_KHR_dynamic_rendering_local_read`) for multi-attachment shader sampling via `subpassLoad()`.
- Multi-point light accumulation with distance attenuation and tone mapping in a single fullscreen pass without legacy render passes.

## Acceptance Criteria
- [x] Create G-Buffer images and image views for Position, Normal, Albedo, and Depth targets.
- [x] Construct G-Buffer geometry pipeline configuring 3 color attachments and 1 depth attachment.
- [x] Construct Deferred Lighting pipeline configuring input attachments mapped to G-Buffer targets.
- [x] Pass 1: Render 3D scene geometry writing surface attributes to all G-Buffer attachments.
- [x] Apply regional execution barrier (`vkCmdPipelineBarrier2` with `VK_DEPENDENCY_BY_REGION_BIT`).
- [x] Pass 2: Bind lighting pipeline, execute fullscreen quad drawing, evaluate dynamic point lights with `subpassLoad()`, and write final lit color to swapchain.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: G-Buffer and lighting shaders (`gbuffer.vert`, `gbuffer.frag`, `lighting.vert`, `lighting.frag`).
- `CMakeLists.txt`: Build target configuration.
