# Assignment 48 – Synchronous Graphics/Compute Linearity & Timeline Semaphore Batch Graph (`VK_KHR_timeline_semaphore`)

## Overview
Architect a complete Directed Acyclic Graph (DAG) render scheduler driven exclusively by 64-bit monotonic `VkSemaphore` timeline values, handling complex multi-queue dependencies (Compute Pre-pass $\rightarrow$ Shadow Raster $\rightarrow$ Async SSAO $\rightarrow$ G-Buffer) without host CPU stalls.

## Key Concepts
- Monotonic 64-bit timeline semaphore values (`VkTimelineSemaphoreSubmitInfo`, `VkSemaphoreWaitInfo`).
- GPU-to-GPU timeline orchestration across Graphics and Dedicated Compute queues.
- Eliminating host CPU pipeline bubbles by coordinating work execution purely on GPU engines.
- Ring-buffered frame pacing with host thread non-blocking synchronization.

## Acceptance Criteria
- [x] Create 64-bit timeline semaphores with `VK_SEMAPHORE_TYPE_TIMELINE`.
- [x] Construct an asynchronous dependency graph scheduling work across Graphics and Dedicated Compute queues.
- [x] Advance monotonic timeline milestone points per pass without CPU fence round-trips.
- [x] Synchronize host frame pacing using `vkWaitSemaphores` with ring buffer threshold targets.
- [x] Verify smooth execution and zero GPU pipeline stalling.

## Directory Structure
- `src/main.cpp`: Timeline semaphore graph scheduler and multi-queue host application.
- `CMakeLists.txt`: Build target configuration.
