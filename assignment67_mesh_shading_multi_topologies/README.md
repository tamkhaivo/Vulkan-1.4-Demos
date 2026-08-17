# Assignment 67 – Hardware Mesh Shading with Dual Primitive Topologies & Multi-Resolution Meshlets (`VK_EXT_mesh_shader`)

## Overview
Implement advanced mesh shader primitive generation utilizing dynamic runtime topology selection (points, lines, triangles) and multi-resolution meshlet tessellation directly on the GPU, outputting dynamic vertex and primitive indices per workgroup.

## Key Concepts
- Mesh shader primitive topology configurations (`layout(triangles, max_vertices = N, max_primitives = M) out`).
- Dynamic primitive culling and compacting (`SetMeshOutputsEXT`).
- Procedural multi-resolution meshlet LOD generation in task/amplification shaders.
- Outputting per-primitive attributes and hardware primitive cull flags (`gl_MeshPrimitivesEXT`).

## Acceptance Criteria
- [x] Configure mesh shading graphics pipelines for complex procedural geometry.
- [x] Implement a task shader evaluating distance and screen-space size to emit variable mesh task counts.
- [x] Write a mesh shader generating vertex and primitive lists with dynamic triangle strip topology compaction.
- [x] Export per-primitive attributes (primitive IDs, face normals) directly to the fragment shader.
- [x] Verify correct geometry rasterization across multiple LOD ranges with zero invalid primitive artifacts.

## Directory Structure
- `src/main.cpp`: Multi-topology meshlet shading host application.
- `CMakeLists.txt`: Build target configuration.
