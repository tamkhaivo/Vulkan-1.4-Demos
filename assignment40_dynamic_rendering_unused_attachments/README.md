# Assignment 40 – Dynamic Rendering Unused Attachments & Modular Passes (`VK_EXT_dynamic_rendering_unused_attachments`)

## Overview & Architectural Critique
When developing modular rendering pipelines (e.g. forward lighting with optional velocity vectors or depth-only prepasses), standard dynamic rendering required compiling distinct `VkPipeline` variants for each combination of bound attachments. Binding fewer attachments than declared caused validation errors.

In Vulkan 1.4, **Dynamic Rendering Unused Attachments (`VK_EXT_dynamic_rendering_unused_attachments` / Vulkan 1.4 Core)** allows pipelines created with $N$ color attachments to be executed in dynamic rendering passes where certain attachments are set to `VK_ATTACHMENT_UNUSED` / `VK_NULL_HANDLE` without causing pipeline incompatibility or recompilation.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_dynamic_rendering_unused_attachments` Feature**: `dynamicRenderingUnusedAttachments = VK_TRUE`.
- **`VK_ATTACHMENT_UNUSED`**: Specifying `imageView = VK_NULL_HANDLE` and `format = VK_FORMAT_UNDEFINED` for unneeded render targets in `VkRenderingInfo`.
- **PSO Reuse**: Executing a 4-target G-Buffer pipeline in a 2-target or 1-target pass without recreating pipelines.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Pipeline was compiled targeting 3 Color Attachments (Color 0: Albedo, Color 1: Normals, Color 2: Velocity)

// 2. Dynamic Rendering Pass selectively disabling Attachment 1 & 2 without PSO recompilation
VkRenderingAttachmentInfo activeColorAttachments[3] = {
    // Attachment 0: Active Swapchain Color
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.1f, 0.1f, 0.1f, 1.0f }}}
    },
    // Attachment 1: Unused (Null Image View)
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = VK_NULL_HANDLE, // Explicitly Unused
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    },
    // Attachment 2: Unused (Null Image View)
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = VK_NULL_HANDLE, // Explicitly Unused
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    }
};

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 3,
    .pColorAttachments = activeColorAttachments
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, multiTargetPipeline); // Bound without modification!
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_dynamic_rendering_unused_attachments` on physical device.
- [x] Create a multi-attachment graphics pipeline targeting 3+ color attachments.
- [x] Execute dynamic rendering passes with arbitrary subsets of attachments set to `VK_NULL_HANDLE`.
- [x] Verify flawless rendering and 100% clean Vulkan validation layer output.

## Directory Structure
- `src/main.cpp`: Unused attachments dynamic rendering application.
- `shaders/unused_pass.vert`, `shaders/unused_pass.frag`: Multi-target shaders.
- `CMakeLists.txt`: Build target configuration.
