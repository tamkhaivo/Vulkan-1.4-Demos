# Assignment 41 – Multi-View Stereo & Foveated VR Rendering (`VK_KHR_multiview` / Vulkan 1.4 Core)

## Overview & Architectural Critique
Virtual Reality (VR) and stereo rendering require rasterizing the 3D scene from two distinct camera viewpoints (left eye, right eye). Recording duplicate draw calls for each eye doubles CPU command recording overhead and increases GPU vertex processing bottlenecks.

In Vulkan 1.4, **Multi-View Rendering (`VK_KHR_multiview` / Vulkan 1.4 Core)** enables rendering to multiple layered image views simultaneously from a single draw call. By configuring a `viewMask` (e.g. `0b0011` for 2 eyes) in `VkRenderingInfo`, the GPU hardware replicates primitives to each layer, allowing shaders to index eye-specific matrices using `gl_ViewIndex` with a 50% CPU recording reduction.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Multiview Feature**: `multiview = VK_TRUE`.
- **Dynamic Rendering Multiview Setup**: Setting `viewMask = 0x3` in `VkRenderingInfo` and `VkPipelineRenderingCreateInfo.viewMask`.
- **Layered 2D Texture Array**: Attachments created with `arrayLayers = 2` (Layer 0 = Left Eye, Layer 1 = Right Eye).
- **GLSL View Index**: `layout(std140, set = 0, binding = 0) uniform VRCamera { mat4 eyeViewProj[2]; };` indexed via `gl_ViewIndex`.

## Concrete Implementation Example (GLSL Vertex Shader & C++ Host)

### GLSL Vertex Shader (`vr.vert`)
```glsl
#version 460
#extension GL_EXT_multiview : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform VRCameraBlock {
    mat4 eyeViewProj[2]; // Index 0: Left Eye, Index 1: Right Eye
} camera;

layout(location = 0) out vec3 outNormal;

void main() {
    // Hardware automatically assigns gl_ViewIndex based on viewMask
    gl_Position = camera.eyeViewProj[gl_ViewIndex] * vec4(inPosition, 1.0);
    outNormal = inNormal;
}
```

### C++ Host Command Recording
```cpp
// Dynamic Rendering with Multiview Mask (Bit 0 + Bit 1 enabled = 0x3)
VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, eyeExtent },
    .layerCount = 1,
    .viewMask = 0x3, // Stereo dual-eye broadcast
    .colorAttachmentCount = 1,
    .pColorAttachments = &stereoColorAttachment
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stereoPipeline);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0); // Single draw call renders BOTH eyes!
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Enable `multiview` feature on physical device.
- [x] Allocate 2-layer 2D Texture Array color and depth attachments.
- [x] Configure `viewMask = 0x3` in `VkPipelineRenderingCreateInfo` and `VkRenderingInfo`.
- [x] Write GLSL vertex shader routing vertices via `gl_ViewIndex`.
- [x] Render stereo 3D scene from a single draw call with 60+ FPS per eye and zero validation warnings.

## Directory Structure
- `src/main.cpp`: Multi-view stereo host application.
- `shaders/vr_multiview.vert`, `shaders/vr_multiview.frag`: Stereo shaders.
- `CMakeLists.txt`: Build target configuration.
