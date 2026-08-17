# Assignment 35 – Shader Execution Reordering (SER) & Position Fetch (`VK_NV_shader_execution_reorder` / `VK_KHR_ray_tracing_position_fetch`)

## Overview
Mitigate ray tracing execution divergence in complex path tracers by reordering divergent threads using `hitObjectNV` / `reorderThreadNV()`, and fetch 3D triangle vertex positions directly from acceleration structure leaf nodes.

## Key Concepts
- `VK_NV_shader_execution_reorder` and `VK_KHR_ray_tracing_position_fetch` feature enablement.
- Divergence mitigation in path tracing via GLSL `hitObjectNV` and `reorderThreadNV()`.
- Coalescing SIMD execution lanes before executing expensive material evaluation shaders.
- Extracting vertex positions from BVHs (`OpRayQueryGetIntersectionTriangleVertexPositionsKHR` / `gl_HitTriangleVertexPositionsKHR`) without manual vertex buffer lookups.

## Acceptance Criteria
- [x] Query and enable `shaderExecutionReorder` and `rayTracingPositionFetch` features.
- [x] Write ray tracing / compute shader utilizing `hitObjectNV` to trace rays and separate ray sorting from shading.
- [x] Call `reorderThreadNV()` to group coherent rays before material evaluation.
- [x] Use ray tracing position fetch to extract intersection triangle vertex coordinates directly from the acceleration structure.
- [x] Render complex path-traced visual output with optimized SIMD execution coherence.

## Directory Structure
- `src/main.cpp`: SER and position fetch host application.
- `shaders/`: GLSL shaders with `GL_NV_shader_execution_reorder` and position fetch.
- `CMakeLists.txt`: Build target configuration.
