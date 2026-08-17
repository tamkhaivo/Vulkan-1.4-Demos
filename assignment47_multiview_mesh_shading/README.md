# Assignment 47 – Multi-View Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader` + `VK_KHR_multiview`)

## Overview
Combine Task/Mesh Shading with Vulkan 1.4 Multi-View rendering (`VK_KHR_multiview`) to execute single-pass stereoscopic/VR cluster culling and meshlet generation across multiple viewpoints using `gl_ViewIndex`.

## Key Concepts
- Integrating `VK_EXT_mesh_shader` with `VK_KHR_multiview` in Vulkan 1.4.
- `gl_ViewIndex` propagation inside Task (`.task`) and Mesh (`.mesh`) shader stages.
- Multi-view dual-frustum cone culling in task shaders against multiple eye view matrices.
- Rendering stereo/VR targets in a single draw call with 50% CPU recording reduction.

## Acceptance Criteria
- [x] Query and enable `multiview` and `meshShader` physical device features.
- [x] Configure dynamic rendering attachment with 2D array layers and `viewMask = 0b11`.
- [x] Implement Task shader performing SIMD subgroup dual-frustum culling across viewpoints.
- [x] Emit meshlet primitives directly to multi-view layered render targets via `vkCmdDrawMeshTasksEXT`.
- [x] Verify single-pass stereo draw execution without validation errors.

## Directory Structure
- `src/main.cpp`: Multi-view mesh shading pipeline setup, viewMask configuration, and execution host application.
- `CMakeLists.txt`: Build target configuration.
