# Assignment 47 – Multi-View Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader` + `VK_KHR_multiview`)

## Overview & Architectural Critique
Rendering stereoscopic VR or multi-view displays using modern task/mesh shading architectures requires executing meshlet cluster culling and geometry amplification across multiple eye viewpoints simultaneously.

In Vulkan 1.4, combining **Mesh Shading (`VK_EXT_mesh_shader`)** with **Multi-View (`VK_KHR_multiview`)** enables task shaders to perform stereo frustum culling for both eyes within a single invocation. The mesh shader routes generated meshlet primitives to the corresponding view layer via `gl_ViewIndex`, eliminating duplicate geometry generation entirely.

## Key Vulkan 1.4 Concepts
- **Multi-View Mesh Pipeline**: Combining `VK_SHADER_STAGE_TASK_BIT_EXT` / `VK_SHADER_STAGE_MESH_BIT_EXT` with `VkPipelineRenderingCreateInfo.viewMask = 0x3`.
- **GLSL Task & Mesh Multi-View**: Accessing `gl_ViewIndex` inside mesh shaders to index eye-specific projection matrices.
- **Stereo Cluster Compaction**: Task shader culling against both eye view frustums before calling `EmitMeshTasksEXT`.

## Concrete Implementation Example (GLSL Mesh Shader `stereo.mesh`)

```glsl
#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_multiview : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

layout(set = 0, binding = 0) uniform StereoCamera {
    mat4 eyeViewProj[2]; // 0: Left Eye, 1: Right Eye
} camera;

layout(location = 0) out vec3 outColor[];

void main() {
    uint laneId = gl_LocalInvocationIndex;
    SetMeshOutputsEXT(3, 1);

    if (laneId < 3) {
        vec3 basePos = getVertexPos(laneId);
        // Transform vertex using the hardware-selected gl_ViewIndex
        gl_MeshVerticesEXT[laneId].gl_Position = camera.eyeViewProj[gl_ViewIndex] * vec4(basePos, 1.0);
        outColor[laneId] = (gl_ViewIndex == 0) ? vec3(1, 0, 0) : vec3(0, 0, 1);
    }

    if (laneId == 0) {
        gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
    }
}
```

## Acceptance Criteria
- [x] Enable `VK_EXT_mesh_shader` and `VK_KHR_multiview` physical device features.
- [x] Configure graphics pipeline with task, mesh, and fragment stages targeting a 2-layer stereo render target (`viewMask = 0x3`).
- [x] Emit meshlet geometry in mesh shader transformed via `gl_ViewIndex`.
- [x] Render stereo meshlet scene from a single draw call with 60+ FPS per eye.

## Directory Structure
- `src/main.cpp`: Multi-view mesh shading host application.
- `shaders/stereo_cluster.task`, `shaders/stereo_cluster.mesh`, `shaders/stereo_cluster.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
