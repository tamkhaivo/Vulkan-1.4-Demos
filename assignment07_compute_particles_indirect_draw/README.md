# Assignment 7 – Compute Particle System with Indirect Draw

## Overview & Architectural Critique
In traditional CPU-driven particle pipelines, the CPU computes particle physics and uploads thousands of vertex positions every frame. This introduces severe PCIe bus bottlenecks and CPU stalls.

In Vulkan 1.4, the entire particle simulation and drawing pipeline is driven on the GPU:
1. **Compute Dispatch (`vkCmdDispatch`)**: Updates particle positions, velocities, and lifetimes stored in a Shader Storage Buffer Object (SSBO).
2. **GPU Indirect Command Generation**: A compute shader writes draw arguments directly into a `VkDrawIndirectCommand` structure in a buffer.
3. **Indirect Rendering (`vkCmdDrawIndirect`)**: The graphics pipeline executes the draw command populated by the GPU, bypassing CPU readback entirely.

## Key Vulkan 1.4 Concepts
- **Compute-to-Graphics Synchronization2 Barrier**: `vkCmdPipelineBarrier2` synchronizing `VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT` (write) with `VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT` (read).
- **Storage Buffers (SSBO)**: `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` mapped for compute simulation.
- **`VkDrawIndirectCommand`**: Struct containing `vertexCount`, `instanceCount`, `firstVertex`, and `firstInstance`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Indirect Draw Command Structure
struct VkDrawIndirectCommand {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

// 2. Command Recording: Compute Dispatch -> Synchronization2 -> Indirect Draw
// Step A: Dispatch Particle Compute Simulation
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescSet, 0, nullptr);
vkCmdDispatch(cmd, (NUM_PARTICLES + 255) / 256, 1, 1);

// Step B: Synchronize Compute SSBO writes with Graphics Indirect Read
VkBufferMemoryBarrier2 bufferBarriers[2] = {
    // Particle SSBO barrier (Compute Write -> Vertex Read)
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .buffer = particleBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    },
    // Indirect Command Buffer barrier (Compute Write -> Indirect Command Read)
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        .buffer = indirectCommandBuffer,
        .offset = 0,
        .size = sizeof(VkDrawIndirectCommand)
    }
};

VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = 2,
    .pBufferMemoryBarriers = bufferBarriers
};
vkCmdPipelineBarrier2(cmd, &depInfo);

// Step C: Render Particles using Dynamic Rendering & vkCmdDrawIndirect
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, particleRenderPipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsDescSet, 0, nullptr);
vkCmdDrawIndirect(cmd, indirectCommandBuffer, 0, 1, sizeof(VkDrawIndirectCommand));
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Allocate device-local SSBOs for particle states and indirect draw commands with `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`.
- [x] Implement compute shader updating 50,000+ particle positions and writing alive particle count into `VkDrawIndirectCommand`.
- [x] Insert `VkBufferMemoryBarrier2` guaranteeing memory visibility between compute shader writes and indirect draw execution.
- [x] Execute graphics pass using `vkCmdDrawIndirect` rendering point/billboard particles.
- [x] Maintain 60+ FPS with zero CPU overhead per frame for particle physics updates.

## Directory Structure
- `src/main.cpp`: GPU compute indirect particles source code.
- `shaders/particles.comp`, `shaders/particles.vert`, `shaders/particles.frag`: Simulation & render shaders.
- `CMakeLists.txt`: Build target configuration.
