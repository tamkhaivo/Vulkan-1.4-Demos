# Assignment 18 – Full Hardware Ray Tracing Pipeline & Shader Binding Tables (`VK_KHR_ray_tracing_pipeline`)

## Overview
Implement a full hardware-accelerated ray tracing pipeline using RayGen, Miss, and Closest-Hit shader stages, Acceleration Structures (BLAS/TLAS), and Shader Binding Tables (SBT).

## Key Concepts
- `VK_KHR_ray_tracing_pipeline` and `VK_KHR_acceleration_structure` feature enablement.
- Shader Binding Table (SBT) memory allocation, group indexing, and stride alignments (`shaderGroupBaseAlignment`, `shaderGroupHandleSize`).
- RayGen (`.rgen`), Miss (`.rmiss`), and Closest-Hit (`.rchit`) shader group compilation.
- Ray dispatch execution with `vkCmdTraceRaysKHR`.

## Acceptance Criteria
- [x] Enable hardware ray tracing pipeline features in logical device creation.
- [x] Build Bottom-Level (BLAS) and Top-Level (TLAS) Acceleration Structures with device addresses.
- [x] Create Ray Tracing Pipeline (`vkCreateRayTracingPipelinesKHR`) with RayGen, Miss, and Hit shader groups.
- [x] Query shader group handles and construct aligned Shader Binding Tables (SBT) in GPU memory.
- [x] Dispatch ray tracing workloads using `vkCmdTraceRaysKHR` to render ray-traced visuals to a storage image.

## Directory Structure
- `src/main.cpp`: Hardware ray tracing pipeline host application.
- `shaders/`: Ray tracing shaders (`raygen.rgen`, `miss.rmiss`, `closesthit.rchit`).
- `CMakeLists.txt`: Build target configuration.
