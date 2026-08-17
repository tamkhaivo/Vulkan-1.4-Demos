# Assignment 60 – Dynamic Graph Execution & Indirect Ray Tracing Dispatch (`VK_KHR_ray_tracing_pipeline` Indirect)

## Overview
Achieve full GPU autonomy in ray tracing by computing ray generation dispatch parameters, acceleration structure selection, and dynamic ray budgets directly on the GPU, executing `vkCmdTraceRaysIndirectKHR` from compute pre-passes.

## Key Concepts
- Indirect ray tracing execution (`vkCmdTraceRaysIndirectKHR`, `VkTraceRaysIndirectCommandKHR`).
- Dynamic ray budget allocation based on screen-space complexity / variance estimates.
- GPU computation of RayGen dimensions ($Width \times Height \times Depth$) written directly to device buffers.
- End-to-end GPU-driven ray tracing pipeline eliminating CPU dispatch recording overhead.

## Acceptance Criteria
- [x] Query support for indirect ray tracing (`VkPhysicalDeviceRayTracingPipelinePropertiesKHR`).
- [x] Allocate GPU indirect buffer containing `VkTraceRaysIndirectCommandKHR`.
- [x] Write compute shader evaluating frame complexity and writing dynamic ray tracing dimensions.
- [x] Execute `vkCmdTraceRaysIndirectKHR` with proper buffer memory barriers.
- [x] Verify adaptive GPU-driven ray tracing execution without validation errors.

## Directory Structure
- `src/main.cpp`: Indirect ray tracing dispatch controller and execution host application.
- `CMakeLists.txt`: Build target configuration.
