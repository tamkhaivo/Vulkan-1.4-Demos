# Assignment 36 – Ray Tracing Motion Blur & Time-Varying BVHs (`VK_NV_ray_tracing_motion_blur`)

## Overview
Implement temporal motion-blurred ray tracing by constructing time-parameterized acceleration structures (motion BLAS & TLAS) and tracing time-interpolated rays to achieve cinematic motion blur in path-traced rendering.

## Key Concepts
- `VK_NV_ray_tracing_motion_blur` extension enablement and device query.
- Time-varying bottom-level acceleration structure (BLAS) geometry with `VkAccelerationStructureGeometryMotionTrianglesDataNV`.
- Top-level acceleration structure (TLAS) instance motion matrix interpolation (`VkAccelerationStructureMotionInstanceNV`).
- Passing time parameters into `traceRayMotionNV()` in RayGen and closest-hit shaders.
- Temporal reconstruction and velocity buffer generation for moving geometry.

## Acceptance Criteria
- [x] Query and enable `VK_NV_ray_tracing_motion_blur` and verify device support for motion acceleration structures.
- [x] Build a motion BLAS with multiple vertex position snapshots across a normalized time interval $[t_0, t_1]$.
- [x] Configure `VkAccelerationStructureMotionInfoNV` for the top-level acceleration structure with moving instances.
- [x] Dispatch ray tracing pipeline with `traceRayMotionNV()` supplying per-pixel randomized time samples.
- [x] Render a dynamic scene with moving primitives displaying temporal ray-traced motion blur.

## Directory Structure
- `src/main.cpp`: Motion BLAS/TLAS setup and ray tracing motion host application.
- `CMakeLists.txt`: Build target configuration.
