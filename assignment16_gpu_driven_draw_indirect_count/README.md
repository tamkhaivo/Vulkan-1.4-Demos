# Assignment 16 – GPU-Driven Scene Culling & Multi-Draw Indirect Count (`VK_KHR_draw_indirect_count`)

## Overview
Implement a fully GPU-driven rendering pipeline where a compute shader performs frustum and occlusion culling, writing visible draw commands and an indirect draw count directly into GPU buffers for consumption by `vkCmdDrawIndexedIndirectCountKHR`.

## Key Concepts
- `VK_KHR_draw_indirect_count` / Vulkan 1.2+ core indirect count rendering.
- GPU Frustum Culling via compute shader test against 6 view frustum planes.
- Dynamic indirect command buffer generation (`VkDrawIndexedIndirectCommand`) into SSBOs.
- Dynamic count buffer storing the number of passing draw commands.
- `vkCmdDrawIndexedIndirectCount` / `vkCmdDrawIndirectCount` executing indirect draws without CPU synchronization.

## Acceptance Criteria
- [x] Enable `drawIndirectCount` feature in `VkPhysicalDeviceVulkan12Features`.
- [x] Allocate GPU storage buffers for instance bounding data, indirect draw commands, and dynamic count atomic counter.
- [x] Run compute shader to evaluate instance visibility and write visible draw commands atomically.
- [x] Issue pipeline barrier synchronizing Compute Shader Write -> Indirect Command Read (`VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT`).
- [x] Render all visible instances with a single `vkCmdDrawIndexedIndirectCount` call.

## Directory Structure
- `src/main.cpp`: GPU-driven indirect count host application.
- `shaders/`: Compute culling shader (`cull.comp`), mesh vertex & fragment shaders.
- `CMakeLists.txt`: Build target configuration.
