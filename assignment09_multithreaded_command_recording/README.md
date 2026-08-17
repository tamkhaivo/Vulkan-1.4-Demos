# Assignment 9 – Multi-Threaded Command Recording with Timeline Semaphores

## Overview & Architectural Critique
In high-performance rendering engines, recording thousands of draw commands on a single CPU thread creates a massive CPU bottleneck. Vulkan was explicitly architected to support parallel command recording across multiple CPU worker threads using **Secondary Command Buffers** and independent `VkCommandPool` instances.

In Vulkan 1.4, **Timeline Semaphores (`VkSemaphoreTypeCreateInfo` with `VK_SEMAPHORE_TYPE_TIMELINE`)** provide monotonic 64-bit counter synchronization across CPU threads and GPU queues. This eliminates the need for binary semaphores and complex fence polling loops, enabling fine-grained lock-free job graphs.

## Key Vulkan 1.4 Concepts
- **Thread-Local Command Pools**: Each CPU worker thread allocates secondary command buffers from its own `VkCommandPool` (with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`).
- **Dynamic Rendering Inheritance**: `VkCommandBufferInheritanceRenderingInfo` passed in `VkCommandBufferInheritanceInfo.pNext` to secondary command buffers to inherit dynamic rendering formats.
- **`vkCmdExecuteCommands`**: The primary command buffer records `vkCmdBeginRendering`, executes parallel secondary command buffers via `vkCmdExecuteCommands`, and finishes with `vkCmdEndRendering`.
- **Timeline Semaphore Synchronization**: Utilizing `vkWaitSemaphores` and `vkSignalSemaphore` with 64-bit payload values for CPU/GPU coordination.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Timeline Semaphore
VkSemaphoreTypeCreateInfo timelineCreateInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    .initialValue = 0
};
VkSemaphoreCreateInfo semInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &timelineCreateInfo
};
vkCreateSemaphore(device, &semInfo, nullptr, &timelineSemaphore);

// 2. Secondary Command Buffer Recording on Worker Thread
void recordSecondaryPass(VkCommandBuffer secCmd, VkFormat colorFormat, VkFormat depthFormat, uint32_t chunkId) {
    VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkCommandBufferInheritanceInfo inheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = &inheritanceRenderingInfo
    };

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        .pInheritanceInfo = &inheritanceInfo
    };

    vkBeginCommandBuffer(secCmd, &beginInfo);
    // Bind pipeline, descriptor sets, and record draw calls for this chunk
    vkCmdBindPipeline(secCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, chunkPipeline);
    vkCmdDrawIndexed(secCmd, indicesPerChunk, 1, chunkId * indicesPerChunk, 0, 0);
    vkEndCommandBuffer(secCmd);
}

// 3. Primary Command Buffer Execution & Timeline Semaphore Queue Submission
void submitPrimaryWithTimeline(VkCommandBuffer primaryCmd, const std::vector<VkCommandBuffer>& secondaryCmds, uint64_t frameTimelineValue) {
    vkCmdBeginRendering(primaryCmd, &renderInfo);
    vkCmdExecuteCommands(primaryCmd, static_cast<uint32_t>(secondaryCmds.size()), secondaryCmds.data());
    vkCmdEndRendering(primaryCmd);
    vkEndCommandBuffer(primaryCmd);

    VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &frameTimelineValue
    };

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timelineSubmitInfo,
        .commandBufferCount = 1,
        .pCommandBuffers = &primaryCmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &timelineSemaphore
    };

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
}
```

## Acceptance Criteria
- [x] Create dedicated `VkCommandPool` instances for 4+ CPU worker threads.
- [x] Record secondary command buffers in parallel with `VkCommandBufferInheritanceRenderingInfo`.
- [x] Execute recorded secondary command buffers via `vkCmdExecuteCommands` within primary dynamic rendering pass.
- [x] Create a timeline semaphore (`VK_SEMAPHORE_TYPE_TIMELINE`) and track monotonic frame execution counter values.
- [x] Ensure thread safety and verify zero data races with Valgrind / ThreadSanitizer and 100% clean Vulkan validation.

## Directory Structure
- `src/main.cpp`: Multi-threaded recording & timeline semaphore host application.
- `shaders/multithread.vert`, `shaders/multithread.frag`: Mesh shaders.
- `CMakeLists.txt`: Build target configuration.
