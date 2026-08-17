# Assignment 66 – Programmable Rasterization Order & Subpass Shading (`VK_EXT_rasterization_order_attachment_access`)

## Overview & Architectural Critique
Implementing Order-Independent Transparency (OIT), programmable blending algorithms, and dynamic decal compositing traditionally required costly per-pixel linked lists in SSBOs with high memory overhead and sorting passes.

In Vulkan 1.4, **Rasterization Order Attachment Access (`VK_EXT_rasterization_order_attachment_access` / `VK_ARM_raster_order_attachment_access`)** guarantees that fragment shaders read and write to framebuffer attachments in strict primitive submission order per-pixel. This enables fragment shaders to execute in-place read-modify-write loops without subpass barriers or hazards.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_rasterization_order_attachment_access` Features**: `rasterizationOrderColorAttachmentAccess = VK_TRUE`.
- **Pipeline Raster Order Flags**: `VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT`.
- **In-Place Programmable Blending**: Fragment shaders reading their own output attachments deterministically.

## Concrete Implementation Example (GLSL Fragment Shader)

```glsl
#version 460
#extension GL_EXT_shader_framebuffer_fetch : require

layout(location = 0) in vec4 inColor;
layout(location = 0) inout vec4 fragColor; // Direct read-modify-write framebuffer access

void main() {
    // Custom non-commutative Order-Independent / In-Order programmable blend
    vec4 prevColor = fragColor;
    fragColor = vec4(mix(prevColor.rgb, inColor.rgb, inColor.a), 1.0);
}
```

## Acceptance Criteria
- [x] Query and enable `rasterizationOrderColorAttachmentAccess` physical device feature.
- [x] Configure pipeline with `VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT`.
- [x] Implement in-order programmable blending fragment shader.
- [x] Render overlapping transparent surfaces in strict primitive order with zero race conditions.

## Directory Structure
- `src/main.cpp`: Rasterization order application source code.
- `shaders/raster_order.vert`, `shaders/raster_order.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
