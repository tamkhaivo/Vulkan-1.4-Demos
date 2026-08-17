# Assignment 6 – Two-Pass Effect with Dynamic Rendering Local Reads

## Overview & Architectural Critique
In tile-based mobile architectures (and modern desktop GPUs with L2/on-chip tile buffers), reading previously written framebuffer attachments in subsequent rendering stages is critical for deferred shading, tone mapping, and multipass post-processing.

Historically, Vulkan required complex `VkRenderPass` subpass definitions with `inputAttachments`. In Vulkan 1.4, **Dynamic Rendering Local Read (`VK_KHR_dynamic_rendering_local_read`)** enables dynamic rendering to directly sample previously written color and depth attachments on-chip via `subpassLoad()` in GLSL, without requiring legacy subpass structures or external VRAM roundtrips.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_dynamic_rendering_local_read` / Vulkan 1.4 Feature**: `VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR.dynamicRenderingLocalRead = VK_TRUE`.
- **`VkRenderingInputAttachmentInfoKHR`**: Specifying color/depth attachments that will be read back within the dynamic render pass.
- **In-Pass Synchronization**: Inserting `vkCmdPipelineBarrier2` with dependency flags `VK_DEPENDENCY_BY_REGION_BIT` between pass 1 writing and pass 2 local reading.
- **GLSL Subpass Load**: `layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput uInputColor;` with `subpassLoad(uInputColor)`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Pipeline Creation with Local Read Layout
VkRenderingInputAttachmentIndexInfoKHR localReadIndices{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
    .pNext = nullptr,
    .colorAttachmentCount = 1,
    .pColorAttachmentInputIndices = &inputIndex, // Maps attachment 0 to input attachment index 0
    .pDepthInputAttachmentIndex = nullptr,
    .pStencilInputAttachmentIndex = nullptr
};

VkPipelineRenderingCreateInfo renderingCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .pNext = &localReadIndices,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &colorFormat,
    .depthAttachmentFormat = VK_FORMAT_UNDEFINED
};

// 2. Command Recording: Writing Pass 1 -> Region Barrier -> Reading Pass 2
vkCmdBeginRendering(cmd, &renderInfo);

// --- Pass 1: Render 3D Scene to Color Attachment ---
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

// --- In-Pass Region Memory Barrier for On-Chip Local Read ---
VkMemoryBarrier2 localMemoryBarrier{
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .dstAccessMask = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
};
VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT, // Tile-local dependency
    .memoryBarrierCount = 1,
    .pMemoryBarriers = &localMemoryBarrier
};
vkCmdPipelineBarrier2(cmd, &depInfo);

// --- Pass 2: Post-Processing via Local Read subpassLoad() ---
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, postLayout, 0, 1, &inputAttachmentDescSet, 0, nullptr);
vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen triangle

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Enable `VK_KHR_dynamic_rendering_local_read` feature during device initialization.
- [x] Create intermediate offscreen color/depth attachments with `VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`.
- [x] Configure pipeline creation pNext chain with `VkRenderingInputAttachmentIndexInfoKHR`.
- [x] Insert `vkCmdPipelineBarrier2` with `VK_DEPENDENCY_BY_REGION_BIT` between draw passes inside a single `vkCmdBeginRendering` block.
- [x] Execute fullscreen post-process shader utilizing `subpassLoad()` to apply an edge-detection or grayscale filter.
- [x] Validate zero desktop VRAM thrashing and 100% clean Vulkan validation layer output.

## Directory Structure
- `src/main.cpp`: Dynamic rendering local read application source code.
- `shaders/scene.frag`, `shaders/local_read_post.frag`: Shaders demonstrating `subpassLoad`.
- `CMakeLists.txt`: Build target configuration.
