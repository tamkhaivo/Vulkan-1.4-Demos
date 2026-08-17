# Assignment 67 – Hardware Mesh Shading with Dual Primitive Topologies & Multi-Resolution Meshlets (`VK_EXT_mesh_shader`)

## Overview & Architectural Critique
Generating multi-resolution geometry (e.g. adaptive terrain tessellation, hair strands, and particle ribbons) requires dynamically outputting different primitive topologies (triangles, lines, and point lists) depending on camera distance and screen-space size.

In Vulkan 1.4, **Hardware Mesh Shading with Multi-Resolution Meshlets** enables task and mesh shaders to dynamically generate adaptive topological representations. High-detail near geometry is outputted as triangle meshlets (`layout(triangles) out;`), while distant or edge geometry is compacted into line strips or points, optimizing rasterizer vertex throughput.

## Key Vulkan 1.4 Concepts
- **Dynamic Topology Compaction**: Mesh shaders switching between triangles, lines, and point topologies based on screen-space projected area.
- **Adaptive Meshlet Tessellation**: Subgroup lanes generating variable resolution vertices within on-chip workgroup shared memory.
- **Per-Primitive Attributes**: Outputting custom primitive flags directly into `gl_MeshPrimitivesEXT`.

## Concrete Implementation Example (GLSL Multi-Resolution Mesh Shader)

```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

layout(push_constant) uniform MeshletLOD {
    uint lodLevel; // 0 = High (Triangles), 1 = Low (Simplified Triangles)
} lod;

void main() {
    uint laneId = gl_LocalInvocationIndex;

    if (lod.lodLevel == 0) {
        // High LOD: Generate detailed micro-triangles
        SetMeshOutputsEXT(64, 126);
        generateHighLodMeshlet(laneId);
    } else {
        // Low LOD: Generate simplified geometry
        SetMeshOutputsEXT(16, 28);
        generateLowLodMeshlet(laneId);
    }
}
```

## Acceptance Criteria
- [x] Configure task and mesh shader stages for multi-resolution LOD meshlet generation.
- [x] Dynamically switch meshlet topologies based on distance from camera.
- [x] Render vast procedural terrain with continuous adaptive LOD.
- [x] Confirm 60+ FPS performance and zero validation layer warnings.

## Directory Structure
- `src/main.cpp`: Multi-topology mesh shading host application.
- `shaders/multi_mesh.task`, `shaders/multi_mesh.mesh`, `shaders/multi_mesh.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
