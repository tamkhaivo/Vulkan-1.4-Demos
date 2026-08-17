# Assignment 50 – Ray Tracing Shader Execution Graph & Callable Shaders (`VK_KHR_ray_tracing_pipeline` Callable)

## Overview
Build a procedural shading architecture for hardware ray tracing using **Callable Shaders** (`executeCallableKHR()`), allowing polymorphic material evaluation and BRDF models to be executed dynamically inside Closest-Hit and Miss shaders without branching divergence.

## Key Concepts
- Callable shader stage (`VK_SHADER_STAGE_CALLABLE_BIT_KHR`) in `VkRayTracingPipelineCreateInfoKHR`.
- Shader Binding Table (SBT) callable record layouts and indexing.
- Dynamically calling procedural BRDFs (Lambertian, Microfacet GGX, Subsurface Scattering) via `executeCallableKHR(index, payload)`.
- Eliminating monolithic shader branches and compiling isolated material modules.

## Acceptance Criteria
- [x] Query hardware ray tracing pipeline features (`rayTracingPipeline`).
- [x] Compile multiple callable shader modules representing distinct BRDF materials.
- [x] Configure Shader Binding Table with a dedicated Callable section.
- [x] Invoke `executeCallableKHR()` from RayGen/Closest-Hit shaders with structured payloads.
- [x] Verify polymorphic material shading and validation layer compliance.

## Directory Structure
- `src/main.cpp`: Callable shader pipeline configuration and execution host application.
- `CMakeLists.txt`: Build target configuration.
