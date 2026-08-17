# Assignment 22 – Asynchronous Multi-Queue Concurrency & Transfer Overlap

## Overview
Achieve maximum GPU engine saturation by overlapping asynchronous compute workloads and background DMA transfer operations with main graphics rendering across distinct hardware queue families.

## Key Concepts
- Discovering dedicated hardware queue families: Graphics, Async Compute, and DMA Transfer.
- Cross-queue ownership transfer barriers (release and acquire operations via `VkBufferMemoryBarrier2` / `VkImageMemoryBarrier2`).
- Timeline semaphores (`VkSemaphoreTypeCreateInfo`) for fine-grained multi-queue dependency synchronization.
- Asynchronous compute simulation running concurrently while graphics draws the previous frame's results.

## Acceptance Criteria
- [x] Query and acquire distinct queue families (Graphics, Compute, Transfer) from the physical device.
- [x] Create independent command pools and command buffers for each hardware queue.
- [x] Implement Release-Acquire barrier pairs when passing resources across different queue families.
- [x] Synchronize queue execution using timeline semaphores with monotonically increasing values.
- [x] Demonstrate compute/transfer overlap with active graphics command submission without CPU stalling.

## Directory Structure
- `src/main.cpp`: Multi-queue async compute and transfer host application.
- `shaders/`: Async compute simulation shaders and rendering shaders.
- `CMakeLists.txt`: Build target configuration.
