# Assignment 15 – Hardware Ray Queries & Inline Traversal (`VK_KHR_ray_query`)

## Overview
Perform inline ray tracing traversal (`rayQueryEXT`) directly within compute shaders without creating dedicated ray tracing pipelines (`VkRayTracingPipelineCreateInfoKHR`) or Shader Binding Tables (SBT).

## Key Concepts
- `VK_KHR_ray_query` and `VK_KHR_acceleration_structure` feature enablement (`VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery`).
- Bottom-Level Acceleration Structure (BLAS) and Top-Level Acceleration Structure (TLAS) construction.
- GLSL `GL_EXT_ray_query` inline traversal intrinsics: `rayQueryInitializeEXT`, `rayQueryProceedEXT`, `rayQueryGetIntersectionTypeEXT`.
- Real-time hard shadows and ambient occlusion tested directly inside compute or fragment shaders.

## Acceptance Criteria
- [x] Enable `rayQuery` and `accelerationStructure` features on logical device creation.
- [x] Build GPU Acceleration Structures (BLAS for triangle geometry and TLAS referencing BLAS instance).
- [x] Create descriptor bindings for Top-Level Acceleration Structure (`VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`).
- [x] Execute inline ray queries inside compute/fragment shaders using `rayQueryInitializeEXT` and `rayQueryProceedEXT`.
- [x] Traverse scene acceleration structure and generate ray-queried shading outputs.

## Directory Structure
- `src/main.cpp`: Inline ray query Vulkan 1.4 host application.
- `shaders/`: GLSL shaders with `GL_EXT_ray_query` traversal logic.
- `CMakeLists.txt`: Build target configuration.
