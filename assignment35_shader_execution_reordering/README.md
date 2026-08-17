# Assignment 35 – Shader Execution Reordering (SER) & Position Fetch (`VK_NV_shader_execution_reorder` / `VK_KHR_ray_tracing_position_fetch`)

## Overview & Architectural Critique
In hardware ray tracing, secondary and diffuse rays bounce in completely random directions, causing massive warp divergence as adjacent GPU SIMD threads execute different shaders or traverse different branches of the BVH.

In Vulkan 1.4, **Shader Execution Reordering (SER, `VK_NV_shader_execution_reorder`)** allows shaders to construct `hitObjectNV` records and reorder active warp threads (`reorderThreadNV()`) so threads hitting the same material or spatial region execute concurrently. Combined with **Ray Tracing Position Fetch (`VK_KHR_ray_tracing_position_fetch`)**, shaders can directly extract 3D vertex positions from the BVH leaf without vertex buffer descriptors.

## Key Vulkan 1.4 Concepts
- **`VK_NV_shader_execution_reorder` & `VK_KHR_ray_tracing_position_fetch`**: Features enabled in physical device setup.
- **`hitObjectNV` Intrinsics**: `hitObjectRecordEmptyNV()`, `hitObjectTraceRayNV()`, `hitObjectExecuteShaderNV()`.
- **Thread Reordering**: Calling `reorderThreadNV(hitObj)` to dynamically regroup divergent threads.
- **Position Fetch**: Extracting vertex coordinates from BVH using `fetchMicroTriangleVertexPositionsEXT()` or `OpRayQueryGetIntersectionTriangleVertexPositionsKHR`.

## Concrete Implementation Example (GLSL RayGen Shader with SER)

```glsl
#version 460
#extension GL_NV_shader_execution_reorder : require
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba8) uniform image2D outputImage;

void main() {
    ivec2 pixelCoord = ivec2(gl_LaunchIDEXT.xy);
    vec3 origin = computeCameraOrigin(pixelCoord);
    vec3 direction = computeCameraRay(pixelCoord);

    // 1. Create Hit Object and trace ray into Hit Object (without immediate execution)
    hitObjectNV hitObj;
    hitObjectRecordEmptyNV(hitObj);
    hitObjectTraceRayNV(hitObj, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, origin, 0.001, direction, 1000.0, 0);

    // 2. Reorder active threads based on hit object coherence (spatial / material alignment)
    reorderThreadNV(hitObj);

    // 3. Execute Closest-Hit/Miss shader on reordered, coherent warps
    hitObjectExecuteShaderNV(hitObj, 0);
}
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_shader_execution_reorder` and `VK_KHR_ray_tracing_position_fetch` features.
- [x] Build TLAS with `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR`.
- [x] Implement RayGen shader tracing rays into `hitObjectNV` and regrouping threads via `reorderThreadNV()`.
- [x] Fetch triangle vertex positions directly from acceleration structure in Closest-Hit shader.
- [x] Measure and log 30%+ ray tracing performance improvement in divergent diffuse bouncing scenes.

## Directory Structure
- `src/main.cpp`: SER & Position fetch host application.
- `shaders/ser_raygen.rgen`, `shaders/ser_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
