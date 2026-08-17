# Assignment 70 – Comprehensive Autonomous GPU-Driven Rendering Engine (DGC + Mesh Shaders + Indirect RT + Dynamic Rendering)

## Overview
Unify all advanced Vulkan 1.4 modern graphics architectures into a fully autonomous, zero-CPU-overhead rendering engine. Combine Device Generated Commands (DGC), Task/Mesh Shaders, Hardware Ray Queries/Tracing, and Dynamic Rendering into a single coherent GPU-driven pipeline.

## Key Concepts
- GPU-driven scene traversal, frustum & occlusion culling, and LOD selection via compute shaders.
- Device Generated Commands (DGC) generating draw commands, pipeline state switches, and push constants entirely on device.
- Task/Mesh shading rasterization pass for high-detail geometry.
- Inline ray queries for hybrid shadows and ambient occlusion during dynamic rendering.
- Timeline semaphore multi-engine orchestration and zero-allocation push descriptors.

## Acceptance Criteria
- [x] Initialize Vulkan 1.4 core physical device with DGC, Mesh Shaders, Ray Tracing, and Dynamic Rendering features enabled.
- [x] Implement a compute dispatch generating the indirect token buffer for dynamic multi-pipeline execution.
- [x] Execute DGC command streams invoking task/mesh shaders and traditional draw calls with dynamic pipeline switches.
- [x] Compute inline ray queries within fragment shaders for real-time contact shadows.
- [x] Render complete multi-material scene with zero per-frame CPU command recording overhead, verified with 100% clean validation layers.

## Directory Structure
- `src/main.cpp`: Autonomous GPU-driven hybrid rendering engine host application.
- `CMakeLists.txt`: Build target configuration.
