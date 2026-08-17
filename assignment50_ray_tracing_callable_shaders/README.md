# Assignment 50 – Ray Tracing Callable Shaders & Procedural BRDFs (`VK_KHR_ray_tracing_pipeline`)

## Overview & Architectural Critique
In physically based path tracing engines, implementing diverse material types (Cook-Torrance dielectric, GGX conductor, Disney Principled BSDF, subsurface scattering, procedural car paint) inside a monolithic Closest-Hit shader leads to extreme register pressure and warp execution divergence.

In Vulkan 1.4, **Callable Shaders (`VK_KHR_ray_tracing_pipeline`)** enable dynamic polymorphic execution within ray tracing pipelines. Closest-Hit and Miss shaders invoke specialized callable shader records in the Shader Binding Table (SBT) via `executeCallableKHR()`, decoupling complex material BRDF evaluation from ray traversal.

## Key Vulkan 1.4 Concepts
- **Callable Shader Stage**: `VK_SHADER_STAGE_CALLABLE_BIT_KHR` with `VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR`.
- **Callable SBT Region**: `VkStridedDeviceAddressRegionKHR` passed to `vkCmdTraceRaysKHR`.
- **GLSL Callable Invocation**: `layout(callableDataEXT) ...;` and `executeCallableKHR(materialIndex, callableDataId)`.

## Concrete Implementation Example (GLSL Callable & Closest Hit Shaders)

### Closest Hit Shader (`closesthit.rchit`)
```glsl
#version 460
#extension GL_EXT_ray_tracing : require

struct MaterialPayload {
    vec3 worldNormal;
    vec3 viewDir;
    vec3 lightDir;
    vec3 outBrdfColor;
};

layout(location = 0) callableDataEXT MaterialPayload matData;

void main() {
    matData.worldNormal = getHitNormal();
    matData.viewDir = -gl_WorldRayDirectionEXT;
    matData.lightDir = normalize(vec3(1.0, 2.0, 1.0));

    uint materialId = getMaterialId(); // e.g., 0 = Gold Conductor, 1 = Frosted Glass

    // Polymorphic execution of material BRDF shader via SBT callable table
    executeCallableKHR(materialId, 0);

    // Use matData.outBrdfColor in lighting accumulation...
}
```

### Callable Material Shader (`material_ggx.rcall`)
```glsl
#version 460
#extension GL_EXT_ray_tracing : require

struct MaterialPayload {
    vec3 worldNormal;
    vec3 viewDir;
    vec3 lightDir;
    vec3 outBrdfColor;
};

layout(location = 0) callableDataInEXT MaterialPayload data;

void main() {
    // Evaluate GGX Specular BRDF for Gold Conductor
    vec3 H = normalize(data.viewDir + data.lightDir);
    float NdotH = max(dot(data.worldNormal, H), 0.0);
    float NdotL = max(dot(data.worldNormal, data.lightDir), 0.0);
    
    vec3 F0 = vec3(1.00, 0.78, 0.34); // Gold reflectance
    vec3 fresnel = F0 + (1.0 - F0) * pow(1.0 - max(dot(data.viewDir, H), 0.0), 5.0);
    
    data.outBrdfColor = fresnel * NdotL;
}
```

## Acceptance Criteria
- [x] Create ray tracing pipeline with dedicated callable shader groups.
- [x] Structure and populate Callable SBT region with aligned shader handles.
- [x] Implement multiple distinct callable BRDF shaders (e.g. Lambertian, GGX, Clearcoat).
- [x] Invoke material shaders dynamically via `executeCallableKHR` from Closest-Hit shader.
- [x] Verify correct multi-material rendering and clean validation layer output.

## Directory Structure
- `src/main.cpp`: Ray tracing callable shaders host application.
- `shaders/rt_callable.rgen`, `shaders/rt_callable.rchit`, `shaders/mat_diffuse.rcall`, `shaders/mat_ggx.rcall`: Shaders.
- `CMakeLists.txt`: Build target configuration.
