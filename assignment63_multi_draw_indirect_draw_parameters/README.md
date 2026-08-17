# Assignment 63 – Hardware Primitive Topologies & Multi-Draw Indirect with Draw Parameters (`VK_KHR_shader_draw_parameters`)

## Overview & Architectural Critique
When batching thousands of distinct sub-meshes into a single Multi-Draw Indirect (`vkCmdDrawIndexedIndirect`) call, shaders traditionally lacked built-in variables to determine which specific sub-mesh command within the batch was currently executing.

In Vulkan 1.4, **Shader Draw Parameters (`VK_KHR_shader_draw_parameters` / Vulkan 1.4 Core)** provides hardware builtins: `gl_DrawID`, `gl_BaseVertex`, and `gl_BaseInstance`. Shaders use `gl_DrawID` directly to fetch per-draw material properties, transforms, and bounding boxes from storage buffers without needing per-draw push constants or uniform offsets.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Draw Parameters**: `VkPhysicalDeviceShaderDrawParametersFeatures.shaderDrawParameters = VK_TRUE`.
- **GLSL Builtins**: `gl_DrawID` (indexing current draw in `vkCmdDrawIndexedIndirect`), `gl_BaseVertex`, `gl_BaseInstance`.
- **Zero-Rebind Batching**: Issuing a single multi-draw indirect command covering thousands of heterogeneous meshes.

## Concrete Implementation Example (GLSL Vertex Shader with `gl_DrawID`)

```glsl
#version 460
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

struct DrawData {
    mat4 modelMatrix;
    vec4 materialColor;
    uint textureIndex;
};

layout(std430, set = 0, binding = 0) readonly buffer SceneDrawData {
    DrawData draws[];
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;

void main() {
    // Directly index draw parameters using hardware gl_DrawID
    DrawData data = draws[gl_DrawID];

    gl_Position = globalUBO.viewProj * data.modelMatrix * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outColor = data.materialColor;
}
```

## Acceptance Criteria
- [x] Enable `shaderDrawParameters` feature on physical device.
- [x] Allocate storage buffer holding per-draw transform and material data.
- [x] Implement vertex shader accessing `gl_DrawID` to fetch per-draw parameters.
- [x] Issue a single `vkCmdDrawIndexedIndirect` with 1,000+ draw commands.
- [x] Render all sub-meshes with distinct transforms/colors and clean validation output.

## Directory Structure
- `src/main.cpp`: Multi-draw indirect draw parameters host application.
- `shaders/draw_params.vert`, `shaders/draw_params.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
