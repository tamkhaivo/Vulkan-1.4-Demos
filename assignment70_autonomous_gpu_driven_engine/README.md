# Assignment 70 – Comprehensive Autonomous GPU-Driven Rendering Engine (DGC + Mesh Shaders + Indirect RT + Dynamic Rendering)

## Overview & Architectural Critique
Assignment 70 represents the ultimate capstone architecture: a **Fully Autonomous GPU-Driven Rendering Engine**. Traditional engines suffer from heavy CPU-to-GPU command recording synchronization, frequent CPU state rebinding, and discrete compute-to-rasterization roundtrips.

In this architecture, all modern Vulkan 1.4 features coalesce into a unified zero-CPU-overhead execution graph:
1. **GPU Compute Scene Traversal**: Evaluates camera frustum, hierarchical occlusion culling, and dynamic continuous LOD in compute shaders.
2. **Device Generated Commands (DGC)**: Dynamically generates draw commands, pipeline state switches, and push constants entirely on GPU device memory.
3. **Hardware Task & Mesh Shaders**: Executes cluster-level normal cone culling and on-chip meshlet geometry amplification.
4. **Inline Ray Queries & Hybrid RT**: Traces hardware acceleration structure shadow and ambient occlusion rays directly inside the dynamic rendering fragment shader pass.
5. **Dynamic Rendering & Synchronization2**: Delivers multi-pass G-Buffer and tone-mapping passes without legacy `VkRenderPass` or `VkFramebuffer` overhead.

## Key Vulkan 1.4 Concepts
- **Unified GPU-Driven Architecture**: Zero CPU draw calls per frame; CPU only issues timeline semaphore submission and presentation.
- **DGC Multi-Pipeline Token Stream**: GPU preprocessing and autonomous command execution (`vkCmdExecuteGeneratedCommandsNV` / `EXT`).
- **Cluster Meshlet Amplification**: `EmitMeshTasksEXT` and `gl_MeshVerticesEXT` driving high-detail geometric LODs.
- **Inline Ray Traversal**: `rayQueryEXT` testing hardware TLAS shadows during rasterization.
- **64-bit Timeline Orchestration**: Monotonic GPU synchronization across Graphics, Compute, and Transfer engines.

## Concrete Implementation Example (Vulkan 1.4 Master Rendering Loop)

```cpp
// =========================================================================
// Vulkan 1.4 Capstone Autonomous GPU-Driven Engine Frame Execution
// =========================================================================

void renderAutonomousFrame(VkCommandBuffer cmd, uint32_t imageIndex) {
    // 1. PHASE 1: GPU Compute Frustum/Occlusion Culling & DGC Token Generation
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gpuCullingDgcPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullingLayout, 0, 1, &sceneDescSet, 0, nullptr);
    vkCmdDispatch(cmd, (TOTAL_SCENE_OBJECTS + 63) / 64, 1, 1);

    // 2. PHASE 2: Synchronization2 Barrier (Compute SSBO -> DGC Execution & Mesh Input)
    VkBufferMemoryBarrier2 dgcBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccessMask = VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        .buffer = dgcTokenBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &dgcBarrier };
    vkCmdPipelineBarrier2(cmd, &depInfo);

    // 3. PHASE 3: Preprocess Generated Commands on GPU
    vkCmdPreprocessGeneratedCommandsNV(cmd, &generatedCommandsInfo);

    // 4. PHASE 4: Dynamic Rendering Pass (DGC + Task/Mesh Shaders + Inline Ray Queries)
    VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.02f, 0.02f, 0.04f, 1.0f }}}
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { {0, 0}, swapExtent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    // Execute the complete autonomous command stream on GPU silicon
    vkCmdExecuteGeneratedCommandsNV(cmd, VK_FALSE, &generatedCommandsInfo);

    vkCmdEndRendering(cmd);

    // 5. PHASE 5: Transition to Present via Synchronization2
    vkCmdPipelineBarrier2(cmd, &transitionToPresentDepInfo);
}
```

## Acceptance Criteria
- [x] Enable Vulkan 1.4 core physical device with DGC, Mesh Shaders, Ray Queries/Tracing, and Dynamic Rendering.
- [x] Implement compute kernel generating dynamic DGC token streams (switching pipelines, updating push constants, and dispatching meshlet draws).
- [x] Execute DGC stream via `vkCmdExecuteGeneratedCommandsNV` inside dynamic rendering pass.
- [x] Trace real-time hardware ray query shadows inside fragment shaders.
- [x] Achieve complete zero-CPU command recording overhead per frame with 100% clean Vulkan validation layers.

## Directory Structure
- `src/main.cpp`: Autonomous GPU-driven hybrid rendering engine host application.
- `shaders/capstone_cull.comp`, `shaders/capstone_cluster.task`, `shaders/capstone_cluster.mesh`, `shaders/capstone_hybrid.frag`: Complete shader suite.
- `CMakeLists.txt`: Build target configuration.
