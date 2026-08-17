# Assignment 34 – Dynamic Rendering Multi-Pass Suspend, Resume & Feedback Loops

## Overview
Span dynamic rendering render passes across multiple command buffers using `VK_RENDERING_SUSPENDING_BIT` and `VK_RENDERING_RESUMING_BIT`, and execute programmable framebuffer feedback loops (`VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT`).

## Key Concepts
- `VK_RENDERING_SUSPENDING_BIT` and `VK_RENDERING_RESUMING_BIT` in `VkRenderingInfo::flags`.
- Multi-command-buffer render pass continuation without costly tile flushes to VRAM.
- `VK_EXT_attachment_feedback_loop_layout` / `VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT`.
- Fragment shader direct sampling from the currently bound color/depth attachment.

## Acceptance Criteria
- [x] Enable `attachmentFeedbackLoopDynamicRendering` features in logical device initialization.
- [x] Begin dynamic rendering in Primary Command Buffer 1 with `VK_RENDERING_SUSPENDING_BIT`.
- [x] Resume dynamic rendering in Primary Command Buffer 2 with `VK_RENDERING_RESUMING_BIT`.
- [x] Configure attachment image with `VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT` layout.
- [x] Sample and write to the same render target attachment in the fragment shader without validation hazards.

## Directory Structure
- `src/main.cpp`: Suspend/Resume dynamic rendering host application.
- `shaders/`: GLSL shaders for attachment feedback loop processing.
- `CMakeLists.txt`: Build target configuration.
