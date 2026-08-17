# Assignment 11 – Modern Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader`)

## Overview
Implement a modern mesh shading graphics pipeline using task and mesh shaders (`VK_EXT_mesh_shader`) to bypass the fixed-function input assembly and vertex buffer fetching stages.

## Key Concepts
- `VK_EXT_mesh_shader` feature enablement (`VkPhysicalDeviceMeshShaderFeaturesEXT::taskShader`, `meshShader`).
- GLSL Task Shader (`.task`) for coarse-level cluster culling and meshlet workgroup emission via `EmitMeshTasksEXT`.
- GLSL Mesh Shader (`.mesh`) outputting micro-mesh vertex and primitive topology using `SetMeshOutputsEXT`, `gl_MeshVerticesEXT`, and `gl_PrimitiveTriangleIndicesEXT`.
- Dynamic rendering drawing commands using `vkCmdDrawMeshTasksEXT`.
- Zero vertex buffer binding overhead with GPU procedural generation.

## Acceptance Criteria
- [x] Enable `VK_EXT_mesh_shader` with `taskShader` and `meshShader` feature flags in device initialization.
- [x] Load and bind `vkCmdDrawMeshTasksEXT` dynamically or statically.
- [x] Graphics pipeline is created with task and mesh shader stages without vertex input state (`pVertexInputState = nullptr`).
- [x] Task and mesh shaders execute cooperatively, dispatching meshlet workgroups to generate procedural geometry.
- [x] Frame draws smoothly using `vkCmdDrawMeshTasksEXT` inside dynamic rendering (`vkCmdBeginRendering`).

## Directory Structure
- `src/main.cpp`: Mesh and task shading Vulkan 1.4 host application.
- `shaders/`: Task, mesh, and fragment shaders (`mesh_shader.task`, `mesh_shader.mesh`, `mesh_shader.frag`).
- `CMakeLists.txt`: Build target configuration.
