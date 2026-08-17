# Assignment 23 – Hardware Occlusion Queries & Conditional Rendering (`VK_EXT_conditional_rendering`)

## Overview
Skip expensive rendering passes and draw calls entirely on the GPU with zero CPU latency using hardware occlusion queries and conditional rendering predicates.

## Key Concepts
- `VK_EXT_conditional_rendering` feature enablement (`VkPhysicalDeviceConditionalRenderingFeaturesEXT::conditionalRendering`).
- GPU Occlusion Query Pools (`VK_QUERY_TYPE_OCCLUSION`) recording passed fragment counts.
- Copying query results directly into GPU predicate buffers (`vkCmdCopyQueryPoolResults`).
- Conditional execution blocks (`vkCmdBeginConditionalRenderingEXT` / `vkCmdEndConditionalRenderingEXT`).
- Inverted predicate flags to skip occluded meshes automatically.

## Acceptance Criteria
- [x] Enable `conditionalRendering` extension and device features.
- [x] Load conditional rendering function pointers (`vkCmdBeginConditionalRenderingEXT`, `vkCmdEndConditionalRenderingEXT`).
- [x] Create an occlusion query pool and render bounding box geometry to test visibility.
- [x] Copy occlusion query results into a predicate buffer with `VK_QUERY_RESULT_WAIT_BIT` / 64-bit offsets.
- [x] Wrap detailed mesh draw commands inside conditional rendering blocks, verifying occluded draws are skipped on GPU.

## Directory Structure
- `src/main.cpp`: Conditional rendering host application.
- `shaders/`: GLSL shaders for bounding box testing and full mesh rendering.
- `CMakeLists.txt`: Build target configuration.
