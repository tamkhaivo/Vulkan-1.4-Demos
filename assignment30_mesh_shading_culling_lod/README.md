# Assignment 30 – Advanced Mesh Shading Cluster Culling & LOD Morphing (`VK_EXT_mesh_shader`)

## Overview
Implement advanced meshlet-level geometry pipelines featuring backface cone culling, view-frustum bounding sphere culling in task shaders, and dynamic continuous LOD morphing.

## Key Concepts
- Sub-object cluster culling in Task/Amplification shaders.
- Normal cone / backface culling (`coneApex`, `coneAxis`, `coneCutoff`).
- View-frustum sphere intersection tests before launching mesh shader workgroups.
- Subgroup ballot compaction to emit only visible meshlet payloads via `EmitMeshTasksEXT`.
- Dynamic meshlet LOD selection.

## Acceptance Criteria
- [x] Implement Task Shader performing bounding sphere frustum culling and normal cone backface culling.
- [x] Use `subgroupBallot` to compact visible meshlet indices into task payload.
- [x] Emit exact visible meshlet count with `EmitMeshTasksEXT(visibleCount, 1, 1)`.
- [x] Generate meshlet geometry dynamically in Mesh Shader based on payload.
- [x] Render complex high-density geometry with substantial GPU triangle savings from cluster culling.

## Directory Structure
- `src/main.cpp`: Advanced mesh shading host application.
- `shaders/`: Task culling shader (`cull.task`), mesh generation shader (`geometry.mesh`), fragment shader.
- `CMakeLists.txt`: Build target configuration.
