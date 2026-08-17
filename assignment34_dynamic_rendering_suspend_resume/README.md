# Assignment 34 – Dynamic Rendering Suspend/Resume & Attachment Feedback Loops

## Overview & Architectural Critique
In multi-pass rendering (e.g. cascaded shadows, intermediate UI overlays, or progressive blurs), having to finish and restart render passes across multiple command buffers or queues leads to redundant attachment flushes and reloads. Furthermore, reading and writing to the same attachment within a single pass is undefined without feedback loop synchronization.

In Vulkan 1.4, **Dynamic Rendering Suspend/Resume** (`VK_RENDERING_SUSPENDING_BIT` and `VK_RENDERING_RESUMING_BIT`) allows a dynamic render pass to pause across command buffer boundaries and resume seamlessly without clearing or reloading attachments. Additionally, **Attachment Feedback Loops (`VK_EXT_attachment_feedback_loop_layout`)** allows reading from an attachment while concurrently rendering to it under `VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT`.

## Key Vulkan 1.4 Concepts
- **Suspend/Resume Flags**: Passing `flags = VK_RENDERING_SUSPENDING_BIT` in `vkCmdBeginRendering` for pass 1, and `flags = VK_RENDERING_RESUMING_BIT` for pass 2.
- **Attachment Feedback Loop Layout**: Setting `VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT` and `VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT`.
- **Feedback Blending**: In-place color modification without ping-pong buffer duplication.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Command Buffer 1: Begin & Suspend Rendering
VkRenderingInfo renderInfo1{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .flags = VK_RENDERING_SUSPENDING_BIT, // Suspends rendering state
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment
};

vkCmdBeginRendering(cmd1, &renderInfo1);
vkCmdBindPipeline(cmd1, VK_PIPELINE_BIND_POINT_GRAPHICS, basePassPipeline);
vkCmdDrawIndexed(cmd1, baseMeshIndexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd1); // Suspended (not resolved/flushed)

// 2. Command Buffer 2: Resume & Complete Rendering
VkRenderingInfo renderInfo2{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .flags = VK_RENDERING_RESUMING_BIT, // Resumes previous suspended pass
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment
};

vkCmdBeginRendering(cmd2, &renderInfo2);
vkCmdBindPipeline(cmd2, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPassPipeline);
vkCmdDrawIndexed(cmd2, overlayIndexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd2); // Finalized
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_attachment_feedback_loop_layout` features.
- [x] Record multi-command buffer rendering passes using `VK_RENDERING_SUSPENDING_BIT` and `VK_RENDERING_RESUMING_BIT`.
- [x] Configure attachment feedback loop layout and render feedback effect without creating ping-pong textures.
- [x] Submit command buffers sequentially and verify flawless image output with zero validation errors.

## Directory Structure
- `src/main.cpp`: Dynamic rendering suspend/resume host application.
- `shaders/feedback_base.vert`, `shaders/feedback_proc.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
