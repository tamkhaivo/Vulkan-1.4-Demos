# Assignment 40 – Dynamic Rendering Unused Attachments & Modular Render Graphs (`VK_EXT_dynamic_rendering_unused_attachments`)

## Overview
Eliminate pipeline explosion and pipeline recreation when binding varying subsets of render targets in complex deferred/forward rendering graphs using `VK_EXT_dynamic_rendering_unused_attachments` (Vulkan 1.4 core capability).

## Key Concepts
- `VK_EXT_dynamic_rendering_unused_attachments` feature enablement.
- Passing `VK_ATTACHMENT_UNUSED` dynamically inside `VkRenderingInfo` and `VkRenderingAttachmentInfo`.
- Matching pipelines with differing fragment shader output locations without generating validation warnings or requiring separate pipeline variants.
- Per-pass binding flexibility for modular rendering graph passes (e.g., G-Buffer generation skipping specular or velocity targets).

## Acceptance Criteria
- [x] Enable `dynamicRenderingUnusedAttachments` feature on logical device creation.
- [x] Construct a unified multi-render target (MRT) graphics pipeline writing to 4 color outputs.
- [x] Execute dynamic render passes with arbitrary attachments marked as `VK_NULL_HANDLE` and index `VK_ATTACHMENT_UNUSED`.
- [x] Verify clean execution without validation layer errors or pipeline recompilations.
- [x] Render dynamic lighting passes with configurable active target layers.

## Directory Structure
- `src/main.cpp`: Unused attachments dynamic rendering setup and execution host application.
- `CMakeLists.txt`: Build target configuration.
