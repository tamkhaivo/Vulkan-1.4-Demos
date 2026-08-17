# Assignment 11 – Modern Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader`)

## Overview & Architectural Critique
The fixed-function vertex, tessellation, and geometry pipeline is inherently limited by rigid index buffer fetching and fixed hardware topology assembly. **Mesh & Task Shading (`VK_EXT_mesh_shader`)** replaces the entire traditional vertex frontend with a compute-like programming model.

The **Task Shader** (amplification shader) performs coarse cluster/meshlet culling and dynamic LOD selection on GPU compute warps, emitting child mesh workgroups via `EmitMeshTasksEXT`. The **Mesh Shader** generates local vertices and primitive indices directly in workgroup on-chip shared memory, writing output triangles via `gl_MeshVerticesEXT` and `gl_PrimitiveTriangleIndicesEXT` before rasterization.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_mesh_shader` Extension**: Device features `taskShader = VK_TRUE` and `meshShader = VK_TRUE`.
- **Pipeline Stage Flags**: `VK_SHADER_STAGE_TASK_BIT_EXT` and `VK_SHADER_STAGE_MESH_BIT_EXT`.
- **Draw Call**: `vkCmdDrawMeshTasksEXT(cmd, groupCountX, groupCountY, groupCountZ)`.
- **Task Payload**: `struct TaskPayload` shared from task shader to mesh shader using `taskPayloadSharedEXT`.

## Concrete Implementation Example (GLSL & Vulkan 1.4 C++)

### GLSL Mesh Shader (`mesh.mesh`)
```glsl
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

// Per-vertex and per-primitive outputs
layout(location = 0) out vec3 outColor[];

void main() {
    uint laneId = gl_LocalInvocationIndex;
    
    // Define 1 triangle for demonstration per meshlet workgroup
    SetMeshOutputsEXT(3, 1);
    
    if (laneId < 3) {
        vec3 positions[3] = vec3[3](
            vec3(-0.5, -0.5, 0.0),
            vec3( 0.5, -0.5, 0.0),
            vec3( 0.0,  0.5, 0.0)
        );
        vec3 colors[3] = vec3[3](
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 1.0, 0.0),
            vec3(0.0, 0.0, 1.0)
        );
        
        gl_MeshVerticesEXT[laneId].gl_Position = vec4(positions[laneId], 1.0);
        outColor[laneId] = colors[laneId];
    }
    
    if (laneId == 0) {
        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
    }
}
```

### C++ Host Recording
```cpp
// Execute Mesh Shading Pipeline
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshShaderPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

// Dispatch 64 Meshlet Workgroups
vkCmdDrawMeshTasksEXT(cmd, 64, 1, 1);

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_mesh_shader` features (`taskShader` and `meshShader`).
- [x] Create graphics pipeline with task and mesh shader stages, omitting vertex input assembly state (`pVertexInputState = nullptr`).
- [x] Structure mesh geometry into meshlets (max 64 vertices and 126 triangles per meshlet).
- [x] Issue draw command using `vkCmdDrawMeshTasksEXT`.
- [x] Render complex procedural meshlet geometry with 60+ FPS and clean validation layers.

## Directory Structure
- `src/main.cpp`: Mesh & Task shader host application.
- `shaders/cluster.task`, `shaders/cluster.mesh`, `shaders/cluster.frag`: Mesh shading pipeline SPIR-V shaders.
- `CMakeLists.txt`: Build target configuration.
