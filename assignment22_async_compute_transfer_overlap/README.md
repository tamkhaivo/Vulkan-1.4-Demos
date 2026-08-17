# Assignment 22 – Asynchronous Multi-Queue Concurrency & Transfer Overlap

## Overview & Architectural Critique
Modern GPUs contain dedicated hardware execution engines (Graphics Engine, Compute Engine, DMA Copy Engine) that run in parallel. Submitting all compute, transfer, and rendering tasks to a single Graphics Queue causes pipeline bubbles and hardware under-utilization.

In Vulkan 1.4, **Asynchronous Multi-Queue Concurrency** allows simultaneous execution across dedicated Graphics, Compute, and Transfer queue families. Synchronizing resources across queue boundaries requires explicit **Queue Family Ownership Transfers** (matching `VkBufferMemoryBarrier2` or `VkImageMemoryBarrier2` pairs with `srcQueueFamilyIndex` and `dstQueueFamilyIndex`) orchestrated by 64-bit **Timeline Semaphores**.

## Key Vulkan 1.4 Concepts
- **Queue Discovery**: Querying `VkQueueFamilyProperties` to discover dedicated compute and transfer queues.
- **Queue Family Ownership Transfer (Release & Acquire)**:
  - *Release*: Recorded in queue family $A$ (`srcQueueFamilyIndex = A, dstQueueFamilyIndex = B`).
  - *Acquire*: Recorded in queue family $B$ (`srcQueueFamilyIndex = A, dstQueueFamilyIndex = B`).
- **Timeline Semaphore Synchronization**: Cross-queue dependency chaining without CPU-side synchronization.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Release Barrier on Transfer Queue (Uploading data to buffer)
VkBufferMemoryBarrier2 releaseBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
    .dstAccessMask = 0,
    .srcQueueFamilyIndex = transferQueueFamilyIndex,
    .dstQueueFamilyIndex = graphicsQueueFamilyIndex,
    .buffer = sharedBuffer,
    .offset = 0,
    .size = VK_WHOLE_SIZE
};
VkDependencyInfo releaseDep{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = 1,
    .pBufferMemoryBarriers = &releaseBarrier
};
vkCmdPipelineBarrier2(transferCmd, &releaseDep);

// 2. Acquire Barrier on Graphics Queue (Consuming buffer in rendering)
VkBufferMemoryBarrier2 acquireBarrier{
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
    .srcQueueFamilyIndex = transferQueueFamilyIndex,
    .dstQueueFamilyIndex = graphicsQueueFamilyIndex,
    .buffer = sharedBuffer,
    .offset = 0,
    .size = VK_WHOLE_SIZE
};
VkDependencyInfo acquireDep{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .bufferMemoryBarrierCount = 1,
    .pBufferMemoryBarriers = &acquireBarrier
};
vkCmdPipelineBarrier2(graphicsCmd, &acquireDep);

// 3. Submit Transfer and Graphics with Cross-Queue Timeline Semaphore Signaling
// (Transfer queue signals timeline semaphore value X, Graphics queue waits on value X)
```

## Acceptance Criteria
- [x] Query physical device and acquire distinct queue handles for Graphics, Async Compute, and DMA Transfer.
- [x] Record DMA texture/buffer staging uploads on the dedicated Transfer Queue.
- [x] Implement matching Release and Acquire barriers using `VkBufferMemoryBarrier2` across queue families.
- [x] Orchestrate queue submission execution dependencies using monotonic 64-bit Timeline Semaphores.
- [x] Verify overlap in GPU profiler (e.g. RenderDoc / Nsight) and confirm zero synchronization validation errors.

## Directory Structure
- `src/main.cpp`: Multi-queue async compute and transfer overlap application.
- `shaders/async_compute.comp`, `shaders/async_scene.vert`, `shaders/async_scene.frag`: Compute & graphics shaders.
- `CMakeLists.txt`: Build target configuration.
