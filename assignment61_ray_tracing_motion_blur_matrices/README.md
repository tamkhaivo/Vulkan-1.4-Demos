# Assignment 61 – Ray Tracing Partitioned Motion & Matrix Blur (`VK_NV_ray_tracing_motion_blur`)

## Overview
Implement advanced time-dependent hardware ray tracing using multi-matrix interpolated motion blur. Build motion acceleration structures (Motion BLAS and Motion TLAS) with arbitrary time sampling intervals $[t_{min}, t_{max}]$, and compute motion-aware intersection tests in real-time ray generation/closest-hit shaders.

## Key Concepts
- `VK_NV_ray_tracing_motion_blur` and `VkAccelerationStructureMotionInfoNV`.
- `VkAccelerationStructureGeometryMotionTrianglesDataNV` for vertex motion vectors and time intervals.
- Matrix motion vs. SRST (Scale-Rotate-Scale-Translate) decomposed motion transform matrices.
- `traceRayMotionNV()` shader intrinsics sampling time parameter $t \in [0.0, 1.0]$.
- Motion bounding volume hierarchies (BVH) traversal and temporal ray filtering.

## Acceptance Criteria
- [x] Query and enable `VK_NV_ray_tracing_motion_blur` physical device features.
- [x] Construct a Motion BLAS containing dynamic moving geometry with linear transform matrices at $t=0$ and $t=1$.
- [x] Build a Motion TLAS with `VkAccelerationStructureMotionInstanceNV` instances.
- [x] Implement a RayGen shader distributing random time samples $t$ and invoking `traceRayMotionNV()`.
- [x] Verify temporal motion blur rendering without ray acceleration structure rebuild stalls or validation errors.

## Directory Structure
- `src/main.cpp`: Motion blur ray tracing host application.
- `CMakeLists.txt`: Build target configuration.
