# Assignment 65 – Multi-Queue Direct Compute Physics & Graphics Asynchronous Pipeline

## Overview & Architectural Critique
Executing complex compute simulations (e.g. cloth physics, fluid dynamics, N-body particle systems) synchronously on the main graphics queue stalls rasterization and ray tracing passes.

In Vulkan 1.4, an **Asynchronous Multi-Queue Pipeline** separates compute physics execution entirely onto a dedicated asynchronous Compute Queue. The physics simulation runs concurrently alongside graphics passes (shadow maps, G-Buffer), synchronizing only when updated particle buffers are transferred via **Timeline Semaphores** and cross-queue release/acquire memory barriers.

## Key Vulkan 1.4 Concepts
- **Multi-Queue Concurrency**: Dedicated Graphics Queue and Async Compute Queue running simultaneously.
- **Timeline Semaphore Interop**: Compute queue increments timeline value $T$, graphics queue waits on value $T$ before vertex fetching.
- **Cross-Queue Ownership Transfers**: `VkBufferMemoryBarrier2` release on compute queue and acquire on graphics queue.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Compute Queue Submission (Physics Simulation Step)
uint64_t currentPhysicsTimelineVal = ++physicsTimelineCounter;

VkTimelineSemaphoreSubmitInfo computeTimelineInfo{
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues = &currentPhysicsTimelineVal
};

VkSubmitInfo computeSubmit{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &computeTimelineInfo,
    .commandBufferCount = 1,
    .pCommandBuffers = &computeCmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &physicsTimelineSemaphore
};
vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

// 2. Graphics Queue Submission (Waits on Physics Timeline Value)
VkTimelineSemaphoreSubmitInfo graphicsTimelineInfo{
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &currentPhysicsTimelineVal
};

VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
VkSubmitInfo graphicsSubmit{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &graphicsTimelineInfo,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &physicsTimelineSemaphore,
    .pWaitDstStageMask = &waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &graphicsCmd
};
vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, frameFence);
```

## Acceptance Criteria
- [x] Acquire dedicated Graphics and Compute queue handles.
- [x] Implement compute physics simulation running on the compute queue.
- [x] Orchestrate frame dependencies using 64-bit Timeline Semaphores.
- [x] Render dynamic physics-driven meshes concurrently without graphics stalls.
- [x] Verify overlap in GPU profiler with zero validation layer errors.

## Directory Structure
- `src/main.cpp`: Multi-queue async physics application.
- `shaders/cloth_sim.comp`, `shaders/cloth_render.vert`, `shaders/cloth_render.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
