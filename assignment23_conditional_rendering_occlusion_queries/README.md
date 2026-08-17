# Assignment 23 – Hardware Occlusion Queries & Conditional Rendering (`VK_EXT_conditional_rendering`)

## Overview & Architectural Critique
When performing occlusion culling, reading query pool visibility results back to the CPU (`vkGetQueryPoolResults`) introduces a 1–2 frame latency stall, rendering CPU-based draw skipping inefficient for high-speed dynamic scenes.

In Vulkan 1.4, **Conditional Rendering (`VK_EXT_conditional_rendering`)** allows GPU commands (draws, dispatches, clears) to execute or skip conditionally based on a predicate value stored in a `VkBuffer` on the GPU. By copying occlusion query results directly into predicate buffers (`vkCmdCopyQueryPoolResults`), draw calls are skipped entirely on GPU silicon with zero CPU readback latency.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_conditional_rendering` Feature**: `conditionalRendering = VK_TRUE`.
- **Hardware Occlusion Queries**: `vkCmdBeginQuery` and `vkCmdEndQuery` with `VK_QUERY_TYPE_OCCLUSION`.
- **Direct GPU Query Copy**: `vkCmdCopyQueryPoolResults` copying query results to a `VkBuffer` with `VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT`.
- **Conditional Scope**: Wrapping draw calls in `vkCmdBeginConditionalRenderingEXT` and `vkCmdEndConditionalRenderingEXT`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Pass 1: Render Bounding Box with Occlusion Query
vkCmdResetQueryPool(cmd, queryPool, 0, 1);

vkCmdBeginRendering(cmd, &depthOnlyRenderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boundingBoxPipeline);

vkCmdBeginQuery(cmd, queryPool, 0, 0);
vkCmdDrawIndexed(cmd, boxIndexCount, 1, 0, 0, 0);
vkCmdEndQuery(cmd, queryPool, 0);

vkCmdEndRendering(cmd);

// 2. Direct GPU Copy from Query Pool to Predicate Buffer
vkCmdCopyQueryPoolResults(
    cmd,
    queryPool,
    0, 1,
    predicateBuffer,
    0,
    sizeof(uint64_t),
    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
);

// Memory barrier ensuring predicate buffer is ready for conditional rendering
VkBufferMemoryBarrier2 predBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,
    .dstAccessMask = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT,
    .buffer = predicateBuffer,
    .offset = 0,
    .size = sizeof(uint64_t)
};
VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &predBarrier };
vkCmdPipelineBarrier2(cmd, &depInfo);

// 3. Pass 2: Conditional Render of Complex Mesh (Skipped on GPU if predicate is 0)
VkConditionalRenderingBeginInfoEXT condInfo{
    .sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,
    .buffer = predicateBuffer,
    .offset = 0,
    .flags = 0 // 0 means draw if predicate > 0 (visible)
};

vkCmdBeginRendering(cmd, &mainColorRenderInfo);
vkCmdBeginConditionalRenderingEXT(cmd, &condInfo);

vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, detailedMeshPipeline);
vkCmdDrawIndexed(cmd, highPolyIndexCount, 1, 0, 0, 0);

vkCmdEndConditionalRenderingEXT(cmd);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_conditional_rendering` physical device feature.
- [x] Create occlusion query pool (`VK_QUERY_TYPE_OCCLUSION`) and predicate buffer with `VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT`.
- [x] Render bounding geometry to depth buffer within `vkCmdBeginQuery` / `vkCmdEndQuery`.
- [x] Copy query results directly on GPU via `vkCmdCopyQueryPoolResults` into the predicate buffer.
- [x] Wrap expensive multi-draw passes in `vkCmdBeginConditionalRenderingEXT` and verify zero CPU readback stalls.

## Directory Structure
- `src/main.cpp`: Conditional rendering and occlusion query host application.
- `shaders/bounding_box.vert`, `shaders/detailed_mesh.vert`, `shaders/detailed_mesh.frag`: Occlusion test shaders.
- `CMakeLists.txt`: Build target configuration.
