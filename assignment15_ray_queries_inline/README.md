# Assignment 15 – Hardware Ray Queries & Inline Traversal (`VK_KHR_ray_query`)

## Overview & Architectural Critique
While dedicated Ray Tracing Pipelines (`VK_KHR_ray_tracing_pipeline`) require complex Shader Binding Tables (SBT) and separate RayGen/Miss/Closest-Hit shader stages, **Inline Ray Queries (`VK_KHR_ray_query`)** allow any standard shader stage (Compute, Fragment, Vertex, Mesh) to traverse hardware Acceleration Structures directly.

Ray queries are ideal for hybrid rasterization rendering techniques such as real-time ray-traced shadows, ambient occlusion (RTAO), and reflections inside conventional dynamic rendering passes or compute post-processing filters.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_ray_query` Feature**: `VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery = VK_TRUE`.
- **Acceleration Structures**: Bottom-Level (BLAS) for geometric meshes and Top-Level (TLAS) for scene instances (`VkAccelerationStructureKHR`).
- **GLSL Ray Query Traversal**: `rayQueryEXT rq;`, `rayQueryInitializeEXT(...)`, `rayQueryProceedEXT(rq)`, and `rayQueryGetIntersectionTypeEXT(...)`.
- **Visibility Culling**: Setting `gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT` for optimal hard shadow ray casting.

## Concrete Implementation Example (GLSL Fragment Shader)

```glsl
#version 460
#extension GL_EXT_ray_query : require

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(push_constant) uniform ShadowParams {
    vec3 lightDir;
    float lightDistance;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 origin = inWorldPos + inWorldNormal * 0.001; // Epsilon offset to prevent self-intersection
    vec3 direction = normalize(-pc.lightDir);
    float tMin = 0.001;
    float tMax = pc.lightDistance;

    // Initialize inline ray query
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, topLevelAS, 
                         gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT, 
                         0xFF, origin, tMin, direction, tMax);

    // Hardware ray traversal loop
    while (rayQueryProceedEXT(rq)) {
        // Any-hit logic can be handled here if non-opaque
    }

    // Check intersection status
    float shadowFactor = 1.0;
    if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        shadowFactor = 0.2; // Occluded / in shadow
    }

    vec3 albedo = vec3(0.8, 0.8, 0.8);
    float nDotL = max(dot(inWorldNormal, direction), 0.0);
    outColor = vec4(albedo * (nDotL * shadowFactor + 0.1), 1.0);
}
```

## Acceptance Criteria
- [x] Enable `VK_KHR_acceleration_structure` and `VK_KHR_ray_query` physical device features.
- [x] Build hardware BLAS for triangle meshes and TLAS for scene instances.
- [x] Bind TLAS descriptor (`VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`) to fragment shader.
- [x] Implement inline ray query shadow testing in fragment shader with first-hit termination flags.
- [x] Render real-time contact shadows with smooth dynamic camera navigation and zero validation errors.

## Directory Structure
- `src/main.cpp`: Hardware ray query host application.
- `shaders/ray_query_shadow.vert`, `shaders/ray_query_shadow.frag`: Inline ray query shaders.
- `CMakeLists.txt`: Build target configuration.
