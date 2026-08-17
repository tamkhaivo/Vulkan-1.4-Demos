# Assignment 63 – Hardware Primitive Topologies & Multi-Draw Indirect with Draw Parameters (`VK_KHR_shader_draw_parameters`)

## Overview
Leverage built-in shader draw parameters (`gl_BaseVertexARB`, `gl_BaseInstanceARB`, `gl_DrawIDARB`) within multi-draw indirect pipelines to achieve zero-CPU-overhead batch rendering of diverse meshes with per-draw material and transform index lookups.

## Key Concepts
- `VK_KHR_shader_draw_parameters` / Vulkan 1.4 core `shaderDrawParameters`.
- SPIR-V built-in decorations `DrawIndex`, `BaseVertex`, `BaseInstance`.
- Direct indexing into bindless SSBO transform/material arrays using `gl_DrawID`.
- Zero-overhead multi-draw batched execution with `vkCmdDrawIndexedIndirect`.

## Acceptance Criteria
- [x] Enable `shaderDrawParameters` feature in `VkPhysicalDeviceVulkan11Features`.
- [x] Allocate a structured buffer of per-mesh world matrices and PBR material descriptors.
- [x] Record a single `vkCmdDrawIndexedIndirect` call executing multiple disjoint sub-meshes.
- [x] Access per-draw metadata in the vertex/fragment shader using `gl_DrawID` without push constant updates.
- [x] Verify correct transformations and material assignment across all draw calls without validation errors.

## Directory Structure
- `src/main.cpp`: Multi-draw indirect draw parameters host application.
- `CMakeLists.txt`: Build target configuration.
