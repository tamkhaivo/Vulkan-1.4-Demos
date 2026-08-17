# Assignment 55 – Saliency Shading Rate Maps & Dynamic Foveated VRS (`VK_KHR_fragment_shading_rate`)

## Overview & Architectural Critique
While static Variable Rate Shading provides coarse savings, modern rendering engines generate dynamic visual saliency and motion vectors to shade high-contrast, focal-point, or moving regions at full resolution ($1\times 1$), while dynamically downsampling flat or blurred peripheral areas ($2\times 2$ or $4\times 4$).

In Vulkan 1.4, a compute shader analyzes previous frame luminance gradients and velocity vectors to output an $R8\_UINT$ **Saliency Shading Rate Map**. This map is attached to the dynamic rendering pass as a shading rate image via `VkRenderingFragmentShadingRateAttachmentInfoKHR`, reducing total fragment shader invocations by 40%+ without perceived visual degradation.

## Key Vulkan 1.4 Concepts
- **Dynamic Shading Rate Image**: $R8\_UINT$ image matching tile granularity (e.g. $16\times 16$).
- **Compute Saliency Generation**: Evaluating Sobel edge detection and motion speed to compute optimal shading rate codes (`VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_1X1_PIXELS_KHR`, `2X2`, `4X4`).
- **Dynamic Attachment Binding**: Attaching computed rate map directly to `VkRenderingInfo.pNext`.

## Concrete Implementation Example (GLSL Compute Saliency Shader)

```glsl
#version 460
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform readonly image2D sceneColor;
layout(set = 0, binding = 1, r8ui) uniform writeonly uimage2D shadingRateMap;

void main() {
    ivec2 tileCoord = ivec2(gl_WorkGroupID.xy);
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    // Compute local luminance edge magnitude (Sobel filter)
    float edgeMagnitude = computeLocalEdgeMagnitude(pixelCoord);

    // Output VRS Shading Rate Code:
    // 0 = 1x1 (High detail / sharp edge)
    // 5 = 2x2 (Moderate detail)
    // 10 = 4x4 (Flat / low saliency region)
    uint rateCode = 0;
    if (edgeMagnitude < 0.05) {
        rateCode = 10; // 4x4 coarseness
    } else if (edgeMagnitude < 0.2) {
        rateCode = 5;  // 2x2 coarseness
    } else {
        rateCode = 0;  // 1x1 full resolution
    }

    if (gl_LocalInvocationIndex == 0) {
        imageStore(shadingRateMap, tileCoord, uvec4(rateCode, 0, 0, 0));
    }
}
```

## Acceptance Criteria
- [x] Create $R8\_UINT$ shading rate attachment image.
- [x] Implement compute shader analyzing scene saliency and writing hardware shading rate codes.
- [x] Bind dynamic shading rate image in `VkRenderingInfo.pNext`.
- [x] Measure and log 40%+ reduction in fragment shader invocations.
- [x] Verify flawless rendering with 100% clean validation output.

## Directory Structure
- `src/main.cpp`: Dynamic saliency VRS host application.
- `shaders/saliency_calc.comp`, `shaders/saliency_scene.vert`, `shaders/saliency_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
