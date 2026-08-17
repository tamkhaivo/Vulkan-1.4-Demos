# Assignment 9 – Multi-Threaded Command Recording with Timeline Semaphores

## Overview
Scale CPU draw call throughput by recording secondary command buffers in parallel across worker CPU threads, aggregating them into a primary command buffer via `vkCmdExecuteCommands`, and synchronizing execution on the GPU and host using Vulkan 1.4 core Timeline Semaphores.

## Key Concepts
- Thread-isolated `VkCommandPool` instances created per worker thread to avoid host-side lock contention.
- Parallel secondary command buffer recording (`VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT` with `VkCommandBufferInheritanceRenderingInfo`).
- Primary command buffer execution chaining with `vkCmdExecuteCommands`.
- Vulkan 1.4 Timeline Semaphores (`VK_SEMAPHORE_TYPE_TIMELINE`, monotonically increasing 64-bit counter values).
- CPU host queries and waits on timeline semaphores (`vkWaitSemaphores`, `vkGetSemaphoreCounterValue`).

## Acceptance Criteria
- [x] Initialize thread pool and allocate per-thread `VkCommandPool` and secondary `VkCommandBuffer` instances.
- [x] Create timeline semaphore initialized to 0 with `VkSemaphoreTypeCreateInfo` (`VK_SEMAPHORE_TYPE_TIMELINE`).
- [x] Concurrently record secondary command buffers across CPU worker threads, each rendering a subset of 3D objects with `VkCommandBufferInheritanceRenderingInfo`.
- [x] In the primary command buffer, call `vkCmdBeginRendering`, execute all secondary command buffers via `vkCmdExecuteCommands`, and call `vkCmdEndRendering`.
- [x] Submit command buffer signaling the next monotonically increasing timeline semaphore value.
- [x] Demonstrate host synchronization by querying or waiting on the timeline semaphore value before reclaiming resources.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`scene.vert`, `scene.frag`).
- `CMakeLists.txt`: Build target configuration.
