# Assignment 53 – Ray Tracing Position Fetch & BVH Extraction (`VK_KHR_ray_tracing_position_fetch`)

## Overview & Architectural Critique
In conventional hardware ray tracing pipelines, retrieving the 3D vertex positions and geometric triangle coordinates of an intersected primitive requires storing duplicated vertex buffers and indexing into them via descriptors in Closest-Hit / Any-Hit shaders.

In Vulkan 1.4, **Ray Tracing Position Fetch (`VK_KHR_ray_tracing_position_fetch`)** allows ray tracing shaders to fetch the exact 3D vertex positions directly from the internal Acceleration Structure leaf nodes (`OpRayQueryGetIntersectionTriangleVertexPositionsKHR` or GLSL `fetchMicroTriangleVertexPositionsEXT`), eliminating duplicate vertex storage and descriptor binding overhead.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_ray_tracing_position_fetch` Feature**: `rayTracingPositionFetch = VK_TRUE`.
- **AS Build Flag**: `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR` enabling shader readback from BVH data.
- **Descriptorless Position Retrieval**: Accessing `gl_HitTriangleVertexPositionsKHR` in Closest-Hit and Any-Hit shaders.

## Concrete Implementation Example (GLSL Closest-Hit Shader)

```glsl
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_KHR_ray_tracing_position_fetch : require

layout(location = 0) rayPayloadInEXT vec3 hitColor;
hitAttributeEXT vec2 attribs;

void main() {
    // 1. Fetch exact 3D vertex positions directly from BVH (Zero vertex buffer descriptors!)
    vec3 v0 = gl_HitTriangleVertexPositionsKHR[0];
    vec3 v1 = gl_HitTriangleVertexPositionsKHR[1];
    vec3 v2 = gl_HitTriangleVertexPositionsKHR[2];

    // 2. Compute exact geometric face normal from retrieved vertex coordinates
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 geometricNormal = normalize(cross(edge1, edge2));

    hitColor = geometricNormal * 0.5 + 0.5;
}
```

## Acceptance Criteria
- [x] Enable `VK_KHR_ray_tracing_position_fetch` physical device features.
- [x] Build BLAS with `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR`.
- [x] Retrieve 3D vertex positions directly from BVH leaves in Closest-Hit shader.
- [x] Render scene with accurate geometric normals without binding any vertex buffer descriptors.
- [x] Verify 100% clean Vulkan validation layer output.

## Directory Structure
- `src/main.cpp`: Ray tracing position fetch host application.
- `shaders/fetch_raygen.rgen`, `shaders/fetch_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
