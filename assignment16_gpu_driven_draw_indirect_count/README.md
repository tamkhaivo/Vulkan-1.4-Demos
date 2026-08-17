# Assignment 16 – GPU-Driven Scene Culling & Multi-Draw Indirect Count (`VK_KHR_draw_indirect_count`)

## Overview & Architectural Critique
In large open-world rendering, performing view-frustum culling and occlusion culling on the CPU for hundreds of thousands of objects requires massive CPU processing and reading back dynamic draw counts to issue `vkCmdDrawIndexedIndirect`.

In Vulkan 1.4, **Multi-Draw Indirect Count (`VK_KHR_draw_indirect_count` / Vulkan 1.4 Core)** allows the GPU to compute culling in a compute shader, write visible draw commands into a command buffer SSBO, atomically increment a count buffer, and execute `vkCmdDrawIndexedIndirectCount` directly on the GPU without CPU intervention.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Indirect Count**: `vkCmdDrawIndexedIndirectCount` taking `countBuffer` and `countBufferOffset`.
- **Frustum Culling Compute Shader**: Testing object AABBs/bounding spheres against the 6 camera frustum planes.
- **Atomic Draw Compaction**: `atomicAdd(drawCount, 1)` to compact visible instances into contiguous `VkDrawIndexedIndirectCommand` arrays.
- **Synchronization2 Barrier**: Ensuring compute shader indirect writes are available to `VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Frustum Culling Compute Dispatch & Indirect Count Generation
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullingLayout, 0, 1, &cullingDescSet, 0, nullptr);
vkCmdDispatch(cmd, (TOTAL_OBJECTS + 63) / 64, 1, 1);

// 2. Barrier: Compute SSBO writes -> Draw Indirect Execution
VkBufferMemoryBarrier2 barriers[2] = {
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        .buffer = indirectCommandsBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    },
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        .buffer = countBuffer,
        .offset = 0,
        .size = sizeof(uint32_t)
    }
};

VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = 2,
    .pBufferMemoryBarriers = barriers
};
vkCmdPipelineBarrier2(cmd, &depInfo);

// 3. Multi-Draw Indirect Count Execution
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshRenderPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshLayout, 0, 1, &sceneDescSet, 0, nullptr);
vkCmdBindIndexBuffer(cmd, globalIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

vkCmdDrawIndexedIndirectCount(
    cmd,
    indirectCommandsBuffer,
    0,                                  // Offset in indirect command buffer
    countBuffer,
    0,                                  // Offset in count buffer
    MAX_OBJECTS,                        // Max draw count capacity
    sizeof(VkDrawIndexedIndirectCommand)// Stride
);

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Create storage buffers for scene bounding boxes, output `VkDrawIndexedIndirectCommand` arrays, and atomic count.
- [x] Implement compute frustum culling shader testing spheres against camera frustum planes.
- [x] Insert `VkBufferMemoryBarrier2` transitioning compute writes to `VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT`.
- [x] Execute draw loop via `vkCmdDrawIndexedIndirectCount` with dynamic GPU count buffer.
- [x] Demonstrate 100,000+ objects culled and rendered in real-time with sub-millisecond GPU overhead.

## Directory Structure
- `src/main.cpp`: GPU-driven culling application source code.
- `shaders/cull.comp`, `shaders/indirect_mesh.vert`, `shaders/indirect_mesh.frag`: Culling and render shaders.
- `CMakeLists.txt`: Build target configuration.
