# Assignment 8 – Deferred Shading with Multiple Render Targets (Dynamic Rendering Local Reads)

## Overview & Architectural Critique
Forward rendering scales poorly with dynamic lights ($O(\text{geometry} \times \text{lights})$). **Deferred Shading** decouples geometric complexity from lighting calculations ($O(\text{geometry}) + O(\text{lights})$) by writing geometric properties (Albedo, Normals, Roughness/Metallic, Depth) into Multiple Render Targets (G-Buffer) during Pass 1, and evaluating lighting in Pass 2.

In Vulkan 1.4, **Dynamic Rendering with Local Reads** (`VK_KHR_dynamic_rendering_local_read`) eliminates the need to spill G-Buffer data to external VRAM on GPUs that support on-chip attachment reuse, providing massive memory bandwidth savings.

## Key Vulkan 1.4 Concepts
- **Multiple Render Targets (MRT)**: `VkPipelineRenderingCreateInfo` declaring 3 color attachment formats (Albedo: `VK_FORMAT_R8G8B8A8_UNORM`, Normals: `VK_FORMAT_R16G16B16A16_SFLOAT`, Material: `VK_FORMAT_R8G8B8A8_UNORM`) and Depth: `VK_FORMAT_D32_SFLOAT`.
- **Dynamic Rendering Local Read Integration**: Reading all 3 MRTs and Depth attachment locally in Pass 2 with `subpassLoad()` in GLSL.
- **In-Pass Dependency Barrier**: `vkCmdPipelineBarrier2` with `VK_DEPENDENCY_BY_REGION_BIT` ensuring G-Buffer writes are visible to Pass 2 lighting fragment shaders.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. MRT Pipeline Setup (Dynamic Rendering)
VkFormat colorFormats[3] = {
    VK_FORMAT_R8G8B8A8_UNORM,       // Albedo / Specular
    VK_FORMAT_R16G16B16A16_SFLOAT,   // World Normals
    VK_FORMAT_R8G8B8A8_UNORM        // Roughness / Metallic
};

VkPipelineRenderingCreateInfo gbufferPipelineRenderingInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .pNext = nullptr,
    .colorAttachmentCount = 3,
    .pColorAttachmentFormats = colorFormats,
    .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
};

// 2. Command Recording for Deferred Pipeline with Local Reads
VkRenderingAttachmentInfo gbufferColorAttachments[3] = { /* Albedo, Normal, Material Attachment Infos */ };
VkRenderingAttachmentInfo gbufferDepthAttachment = { /* Depth Attachment Info */ };

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, extent },
    .layerCount = 1,
    .colorAttachmentCount = 3,
    .pColorAttachments = gbufferColorAttachments,
    .pDepthAttachment = &gbufferDepthAttachment
};

vkCmdBeginRendering(cmd, &renderInfo);

// --- Phase 1: G-Buffer Generation ---
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline);
vkCmdDrawIndexed(cmd, sceneIndexCount, 1, 0, 0, 0);

// --- Memory Barrier for On-Chip Local Attachment Reading ---
VkMemoryBarrier2 localBarrier{
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .dstAccessMask = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
};
VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    .memoryBarrierCount = 1,
    .pMemoryBarriers = &localBarrier
};
vkCmdPipelineBarrier2(cmd, &depInfo);

// --- Phase 2: Deferred Lighting Calculation ---
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout, 0, 1, &inputAttachmentDescSet, 0, nullptr);
vkCmdDraw(cmd, 3, 1, 0, 0); // Fullscreen quad lighting pass

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Create G-Buffer attachments (Albedo, Normals, Material Properties, Depth) with input attachment usage flags.
- [x] Configure graphics pipeline for 3 MRT outputs with `VkPipelineColorBlendStateCreateInfo` configuring independent attachment write masks.
- [x] Implement in-pass region barrier via `vkCmdPipelineBarrier2` (`VK_DEPENDENCY_BY_REGION_BIT`).
- [x] Implement deferred lighting fragment shader evaluating 32+ point lights by sampling G-Buffer targets via `subpassLoad()`.
- [x] Validate zero visual artifacts, clean validation layer execution, and consistent 60+ FPS performance.

## Directory Structure
- `src/main.cpp`: Deferred shading application source code.
- `shaders/gbuffer.vert`, `shaders/gbuffer.frag`, `shaders/deferred_lighting.frag`: G-Buffer & lighting shaders.
- `CMakeLists.txt`: Build target configuration.
