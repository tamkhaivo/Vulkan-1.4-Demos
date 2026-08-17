# Assignment 48 – Timeline Semaphore Batch Graph Scheduler (`VK_KHR_timeline_semaphore`)

## Overview & Architectural Critique
Complex modern game engines execute DAG-based render graphs spanning G-Buffer passes, async compute culling, ray tracing denoisers, shadow cascades, and post-processing. Coordinating these passes using binary semaphores leads to high CPU tracking overhead, potential deadlocks, and pipeline bubbles.

In Vulkan 1.4, **Timeline Semaphores (`VK_KHR_timeline_semaphore` / Vulkan 1.4 Core)** provide monotonic 64-bit counter values. An entire render graph can be submitted across Graphics, Compute, and Transfer queues in batch submissions, with GPU queues waiting for specific counter thresholds and signaling next-stage milestones with zero CPU blocking.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Timeline Semaphores**: `VkTimelineSemaphoreSubmitInfo` and `vkWaitSemaphores` / `vkSignalSemaphore`.
- **Render Graph Node Dependency**: Node $B$ waits on Timeline Semaphore $S$ reaching value $V$, and signals value $V+1$ upon completion.
- **Lock-Free Multi-Queue Execution**: Continuous asynchronous queue progression without CPU synchronization fences.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Batch Queue Submission with Timeline Semaphore DAG Scheduling
uint64_t computeWaitVal = 10;
uint64_t computeSignalVal = 11;

VkTimelineSemaphoreSubmitInfo computeTimelineInfo{
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &computeWaitVal,
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues = &computeSignalVal
};

VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
VkSubmitInfo computeSubmit{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &computeTimelineInfo,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &dagTimelineSemaphore,
    .pWaitDstStageMask = &waitStage,
    .commandBufferCount = 1,
    .pCommandBuffers = &computeCmd,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &dagTimelineSemaphore
};
vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

// Graphics Queue waits on Compute reaching value 11
uint64_t graphicsWaitVal = 11;
uint64_t graphicsSignalVal = 12;

VkTimelineSemaphoreSubmitInfo graphicsTimelineInfo{
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
    .waitSemaphoreValueCount = 1,
    .pWaitSemaphoreValues = &graphicsWaitVal,
    .signalSemaphoreValueCount = 1,
    .pSignalSemaphoreValues = &graphicsSignalVal
};
// Submit to graphics queue immediately - GPU hardware resolves dependency automatically!
```

## Acceptance Criteria
- [x] Create a 64-bit Timeline Semaphore initialized to 0.
- [x] Structure a 4-node Render Graph (Transfer Upload -> Async Compute Culling -> Deferred Shading -> Post-Process).
- [x] Submit all graph passes asynchronously with monotonic wait and signal timeline values.
- [x] Monitor timeline semaphore progression and confirm zero GPU deadlocks or stalls.

## Directory Structure
- `src/main.cpp`: Timeline semaphore graph scheduler host application.
- `shaders/graph_cull.comp`, `shaders/graph_scene.vert`, `shaders/graph_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
