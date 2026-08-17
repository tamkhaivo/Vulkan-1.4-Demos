# Assignment 61 – Ray Tracing Partitioned Motion & Matrix Blur (`VK_NV_ray_tracing_motion_blur`)

## Overview & Architectural Critique
When rendering fast-moving objects in ray-traced scenes (e.g. spinning rotor blades, racing vehicles), simple linear vertex motion vectors fail to capture curved rotational arcs accurately, resulting in linear shearing artifacts.

In Vulkan 1.4, **Ray Tracing Motion Blur with Partitioned Motion Matrices (`VK_NV_ray_tracing_motion_blur`)** allows specifying multi-matrix motion trajectories (up to $N$ matrix keyframes per instance) within `VkAccelerationStructureMotionInstanceNV`. The ray tracing hardware interpolates between these matrices along the temporal ray interval $[t_{min}, t_{max}]$, providing curvilinear rotational motion blur directly in hardware BVH traversal.

## Key Vulkan 1.4 Concepts
- **`VK_NV_ray_tracing_motion_blur` Feature**: `rayTracingMotionBlur = VK_TRUE`.
- **Matrix Motion Instance Struct**: `VkAccelerationStructureMatrixMotionInstanceNV` containing array of transformation matrices across discrete time steps.
- **GLSL Ray Generation Sampling**: Distributing random temporal samples $t \sim U(0, 1)$ to `traceRayMotionNV()`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Multi-Matrix Motion Instance
VkAccelerationStructureMatrixMotionInstanceNV matrixMotionInstance{
    .transformT0 = glmToVkTransformMatrixKHR(transformAtTime0),
    .transformT1 = glmToVkTransformMatrixKHR(transformAtTime1),
    .instanceCustomIndex = 0,
    .mask = 0xFF,
    .instanceShaderBindingTableRecordOffset = 0,
    .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
    .accelerationStructureReference = blasDeviceAddress
};

// 2. Build Motion TLAS with Matrix Motion Keyframes
VkAccelerationStructureMotionInstanceNV motionInstanceUnion{
    .type = VK_ACCELERATION_STRUCTURE_MOTION_INSTANCE_TYPE_MATRIX_MOTION_NV,
    .flags = 0,
    .data = { .matrixMotionInstance = matrixMotionInstance }
};
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_ray_tracing_motion_blur` features.
- [x] Construct Motion BLAS and Motion TLAS with matrix motion instance keyframes.
- [x] Implement RayGen shader distributing random time samples and calling `traceRayMotionNV()`.
- [x] Render smooth rotational motion blur on high-speed rotating geometry.
- [x] Verify 100% clean validation layer output without acceleration structure stalls.

## Directory Structure
- `src/main.cpp`: Motion blur matrices host application.
- `shaders/matrix_motion.rgen`, `shaders/matrix_motion.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
