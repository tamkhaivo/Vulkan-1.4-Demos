# Assignment 36 – Ray Tracing Motion Blur & Time-Varying BVHs (`VK_NV_ray_tracing_motion_blur`)

## Overview & Architectural Critique
Rendering physically accurate motion blur in real-time ray tracing requires building time-dependent acceleration structures that account for geometric vertex translation and matrix rotation across a camera shutter interval $[t_0, t_1]$.

In Vulkan 1.4, **Ray Tracing Motion Blur (`VK_NV_ray_tracing_motion_blur`)** provides native support for motion BLAS and motion TLAS structures (`VkAccelerationStructureMotionInfoNV`). Ray generation shaders sample arbitrary time intervals $t \in [0, 1]$ and invoke `traceRayMotionNV()`, allowing the hardware BVH traversal cores to interpolate object matrices and vertex positions in hardware.

## Key Vulkan 1.4 Concepts
- **`VK_NV_ray_tracing_motion_blur` Feature**: `rayTracingMotionBlur = VK_TRUE`.
- **Motion Info Struct**: `VkAccelerationStructureMotionInfoNV` with `maxInstances` and motion flags.
- **Motion Instances**: `VkAccelerationStructureMotionInstanceNV` containing matrix motion or SRST (Scale-Rotate-Scale-Translate) decomposed transforms across time steps.
- **Motion Ray Tracing**: `traceRayMotionNV(tlas, rayFlags, cullMask, sbtOffset, sbtStride, missIndex, origin, tMin, dir, tMax, time, payloadIndex)`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Build Motion TLAS with Motion Info
VkAccelerationStructureMotionInfoNV motionInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV,
    .maxInstances = static_cast<uint32_t>(motionInstances.size()),
    .flags = 0
};

VkAccelerationStructureCreateInfoKHR asCreateInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .pNext = &motionInfo, // Attach motion metadata
    .buffer = asBuffer,
    .offset = 0,
    .size = asSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
};
vkCreateAccelerationStructureKHR(device, &asCreateInfo, nullptr, &motionTLAS);

// 2. Dispatch Motion Rays in GLSL RayGen Shader
// traceRayMotionNV(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, origin, 0.001, direction, 1000.0, timeSample, 0);
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_ray_tracing_motion_blur` physical device features.
- [x] Construct Motion BLAS and Motion TLAS with moving geometry transform keyframes.
- [x] Implement RayGen shader sampling temporal shutter times $t \in [0.0, 1.0]$ with random jitter.
- [x] Render smooth photorealistic motion-blurred dynamic geometry.
- [x] Ensure 100% clean validation layer execution without acceleration structure rebuild stalls.

## Directory Structure
- `src/main.cpp`: Motion blur ray tracing host application.
- `shaders/motion_raygen.rgen`, `shaders/motion_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
