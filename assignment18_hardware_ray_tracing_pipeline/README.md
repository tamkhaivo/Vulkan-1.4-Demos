# Assignment 18 – Full Hardware Ray Tracing Pipeline & Shader Binding Tables (`VK_KHR_ray_tracing_pipeline`)

## Overview & Architectural Critique
While inline ray queries are suitable for simple shadow and AO testing, complex global illumination and multi-bounce path tracing require **Full Hardware Ray Tracing Pipelines (`VK_KHR_ray_tracing_pipeline`)**.

A dedicated ray tracing pipeline utilizes dynamic recursion, shader stage execution (`RayGen`, `Closest-Hit`, `Miss`, `Any-Hit`, `Intersection`), and the **Shader Binding Table (SBT)**. The SBT maps individual geometry instances and hit groups to shader handles in GPU memory, requiring precise alignment (`shaderGroupHandleAlignment`, `shaderGroupBaseAlignment`).

## Key Vulkan 1.4 Concepts
- **`VK_KHR_ray_tracing_pipeline` Feature**: `rayTracingPipeline = VK_TRUE`.
- **Shader Binding Table (SBT)**: Structuring `VkStridedDeviceAddressRegionKHR` for RayGen, Miss, Hit, and Callable shader records.
- **Ray Dispatch**: `vkCmdTraceRaysKHR` executing ray generation across image grids $(W, H, D)$.
- **GLSL Ray Tracing Intrinsics**: `traceRayEXT(tlas, rayFlags, cullMask, sbtOffset, sbtStride, missIndex, origin, tMin, dir, tMax, payloadIndex)`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Ray Tracing Pipeline & Shader Groups Setup
VkRayTracingShaderGroupCreateInfoKHR shaderGroups[3] = {
    // Group 0: RayGen
    {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0, // raygen.rgen
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    },
    // Group 1: Miss
    {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1, // miss.rmiss
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    },
    // Group 2: Hit Group (Closest Hit)
    {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 2, // closesthit.rchit
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    }
};

VkRayTracingPipelineCreateInfoKHR pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = 3,
    .pStages = stages,
    .groupCount = 3,
    .pGroups = shaderGroups,
    .maxPipelineRayRecursionDepth = 1,
    .layout = pipelineLayout
};
vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline);

// 2. Dispatch Rays with Shader Binding Table (SBT)
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

vkCmdTraceRaysKHR(
    cmd,
    &raygenSbtRegion,
    &missSbtRegion,
    &hitSbtRegion,
    &callableSbtRegion,
    swapExtent.width,
    swapExtent.height,
    1
);
```

## Acceptance Criteria
- [x] Query hardware ray tracing properties (`shaderGroupHandleSize`, `shaderGroupBaseAlignment`).
- [x] Create ray tracing pipeline with RayGen, Miss, and Closest-Hit shader stages.
- [x] Allocate and pack Shader Binding Table buffer with aligned shader group handles.
- [x] Write ray generation shader shooting primary camera rays and closest-hit shader evaluating lighting and reflections.
- [x] Output ray traced output into storage image (`VK_IMAGE_USAGE_STORAGE_BIT`) and display on swapchain.

## Directory Structure
- `src/main.cpp`: Hardware ray tracing pipeline host application.
- `shaders/raygen.rgen`, `shaders/miss.rmiss`, `shaders/closesthit.rchit`: SPIR-V RT shaders.
- `CMakeLists.txt`: Build target configuration.
