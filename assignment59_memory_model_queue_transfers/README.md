# Assignment 59 – Vulkan Memory Model Queue Ownership Transfers & Lock-Free Ring Buffers (`VK_KHR_vulkan_memory_model`)

## Overview
Build a high-performance lock-free multi-producer multi-consumer (MPMC) GPU ring buffer that transfers data packets across distinct queue families with minimal memory barrier footprint using fine-grained Acquire/Release synchronization.

## Key Concepts
- Vulkan Memory Model acquire/release atomic operations (`atomicLoad`, `atomicStore` with `gl_ScopeQueueFamily` / `gl_ScopeDevice`).
- Explicit ownership transfer barriers (`VkBufferMemoryBarrier2` with `srcQueueFamilyIndex` $\ne$ `dstQueueFamilyIndex`).
- Matching release barrier on exporting queue with acquire barrier on importing queue.
- Lock-free head/tail ring buffer management in SSBOs.

## Acceptance Criteria
- [x] Query and enable `vulkanMemoryModel` and `vulkanMemoryModelDeviceScope`.
- [x] Construct lock-free GPU ring buffer in device storage memory.
- [x] Perform cross-queue ownership transfer using matching release/acquire barriers.
- [x] Verify lock-free atomic push and pop operations under heavy compute concurrency.
- [x] Validate zero memory corruption or race conditions.

## Directory Structure
- `src/main.cpp`: Vulkan Memory Model lock-free ring buffer and queue transfer host application.
- `CMakeLists.txt`: Build target configuration.
