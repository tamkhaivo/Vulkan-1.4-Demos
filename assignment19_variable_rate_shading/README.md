# Assignment 19 – Hardware Variable Rate Shading & Density Maps (`VK_KHR_fragment_shading_rate`)

## Overview & Architectural Critique
In 4K rendering and VR headsets, shading every pixel at full resolution ($1\times 1$) wastes significant GPU fragment shading cycles on peripheral, blurred, or flat-shaded regions.

In Vulkan 1.4, **Variable Rate Shading (VRS, `VK_KHR_fragment_shading_rate`)** allows dynamic control of fragment shader invocation granularity (e.g. $1\times 1, 1\times 2, 2\times 1, 2\times 2, 4\times 2, 4\times 4$ pixels per fragment). Shading rates can be set globally per pipeline/draw (`vkCmdSetFragmentShadingRateKHR`), per primitive in vertex/mesh shaders, or per screen-tile via a **Fragment Shading Rate Attachment (Shading Rate Image)**.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_fragment_shading_rate` Feature**: `pipelineFragmentShadingRate = VK_TRUE`, `attachmentFragmentShadingRate = VK_TRUE`.
- **Shading Rate Attachment**: `VkRenderingFragmentShadingRateAttachmentInfoKHR` in `VkRenderingInfo.pNext` bound with `VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR`.
- **Shading Rate Combiner Operations**: Combining pipeline, primitive, and attachment shading rates using `VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR`, `MIN_KHR`, or `MAX_KHR`.
- **Dynamic Shading Rate**: `vkCmdSetFragmentShadingRateKHR(cmd, &shadingRate, combinerOps)`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Dynamic Shading Rate Attachment in Dynamic Rendering
VkRenderingFragmentShadingRateAttachmentInfoKHR vrsAttachmentInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
    .pNext = nullptr,
    .imageView = vrsDensityImageView,
    .imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
    .shadingRateAttachmentTexelSize = { 16, 16 } // e.g. 16x16 pixel tiles
};

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = &vrsAttachmentInfo,
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment
};

// 2. Set Dynamic Pipeline Shading Rate and Combiners
VkExtent2D baseShadingRate = { 2, 2 }; // 1 fragment invocation per 2x2 pixel footprint
VkFragmentShadingRateCombinerOpKHR combiners[2] = {
    VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, // Pipeline rate vs Primitive rate
    VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR   // Result vs Attachment rate (coarser wins)
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdSetFragmentShadingRateKHR(cmd, &baseShadingRate, combiners);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query physical device fragment shading rate properties (`minFragmentShadingRateAttachmentTexelSize`).
- [x] Create a single-channel `VK_FORMAT_R8_UINT` shading rate density map representing radial foveated rates.
- [x] Attach the density map to dynamic rendering via `VkRenderingFragmentShadingRateAttachmentInfoKHR`.
- [x] Issue `vkCmdSetFragmentShadingRateKHR` with dynamic rate extents and combiner ops.
- [x] Verify visual shading rate reduction and log fragment shader invocation throughput speedup.

## Directory Structure
- `src/main.cpp`: Variable rate shading host application.
- `shaders/vrs_scene.vert`, `shaders/vrs_scene.frag`: Target test shaders.
- `CMakeLists.txt`: Build target configuration.
