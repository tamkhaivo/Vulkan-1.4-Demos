# Assignment 24 – Direct Dynamic Rendering Multisampled Resolves & MSAA

## Overview & Architectural Critique
Multisample Anti-Aliasing (MSAA) renders geometry into 4x/8x multisampled attachments to eliminate geometric aliasing along polygon edges. In traditional Vulkan, resolving multisampled images into single-sampled swapchain images required complex subpass resolve attachments or separate `vkCmdResolveImage` passes.

In Vulkan 1.4, **Dynamic Rendering MSAA Resolve** enables inline on-chip multisample resolves directly within `VkRenderingInfo`. By specifying `resolveMode`, `resolveImageView`, and `resolveImageLayout` in `VkRenderingAttachmentInfo`, the GPU resolves multisampled tiles directly to single-sampled color and depth attachments as tile rendering finishes, with zero external bandwidth penalty.

## Key Vulkan 1.4 Concepts
- **Multisample Dynamic Rendering**: Configuring `VkPipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT`.
- **Inline Color Resolve**: Setting `VkRenderingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT` and supplying `resolveImageView` (single-sampled swapchain image view).
- **Inline Depth Resolve**: Setting `VkRenderingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT | VK_RESOLVE_MODE_MIN_BIT` for depth resolve.
- **Transient Memory Allocation**: Utilizing `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` and `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT` for the multisampled buffer so it never allocates external VRAM.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Color Attachment with Direct Inline Tile Resolve
VkRenderingAttachmentInfo colorAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = msaaColorImageView,                   // 4x MSAA Transient Image
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,        // Hardware box resolve
    .resolveImageView = swapchainImageViews[imageIndex],// 1x Final Swapchain Target
    .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,       // Transient MSAA discarded
    .clearValue = {{{ 0.05f, 0.05f, 0.08f, 1.0f }}}
};

// 2. Configure Depth Attachment with Inline Resolve
VkRenderingAttachmentInfo depthAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = msaaDepthImageView,                   // 4x MSAA Depth
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_MIN_BIT,            // Min-depth resolve
    .resolveImageView = singleSampleDepthImageView,    // 1x Depth Target
    .resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {.depthStencil = { 1.0f, 0 }}
};

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment,
    .pDepthAttachment = &depthAttachment
};

// 3. Render 4x MSAA Scene (Auto-resolved upon vkCmdEndRendering)
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, msaaPipeline);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd); // Inline tile resolve executed here on GPU tile memory
```

## Acceptance Criteria
- [x] Create 4x MSAA transient color and depth images using `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`.
- [x] Configure graphics pipeline with `VK_SAMPLE_COUNT_4_BIT` and sample shading.
- [x] Set up `VkRenderingAttachmentInfo` specifying `VK_RESOLVE_MODE_AVERAGE_BIT` targeting swapchain image view.
- [x] Execute dynamic rendering pass and confirm automatic inline resolve at `vkCmdEndRendering`.
- [x] Verify anti-aliased edge fidelity at 60+ FPS with zero validation layer errors.

## Directory Structure
- `src/main.cpp`: Dynamic rendering MSAA resolve host application.
- `shaders/msaa_scene.vert`, `shaders/msaa_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
