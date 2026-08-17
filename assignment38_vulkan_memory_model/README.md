# Assignment 38 – Vulkan Memory Model & Low-Latency Lock-Free Data Structures (`VK_KHR_vulkan_memory_model`)

## Overview
Master fine-grained memory consistency and synchronization without coarse workgroup barriers using the formal Vulkan Memory Model (`VK_KHR_vulkan_memory_model`), atomic memory semantics (Acquire/Release), and cross-workgroup lock-free GPU queues.

## Key Concepts
- `VK_KHR_vulkan_memory_model` core features (`vulkanMemoryModel`, `vulkanMemoryModelDeviceScope`, `vulkanMemoryModelAvailabilityVisibilityChains`).
- Explicit memory semantics in GLSL (`gl_StorageSemantics`, `gl_SemanticsAcquire`, `gl_SemanticsRelease`, `gl_SemanticsMakeAvailable`, `gl_SemanticsMakeVisible`).
- Cross-workgroup device-scoped atomics (`atomicLoad`, `atomicStore`, `atomicExchange`).
- Implementing lock-free GPU ring buffers, work-stealing queues, and global thread-safe counters without `groupMemoryBarrier()`.

## Acceptance Criteria
- [x] Enable `vulkanMemoryModel` and device-scoped memory model features on the logical device.
- [x] Write a compute shader implementing a multi-workgroup lock-free queue with Acquire-Release memory orderings.
- [x] Demonstrate producer-consumer GPU task execution where compute workgroups produce dynamic items consumed by adjacent workgroups.
- [x] Validate race-condition-free synchronization across all GPU compute units using atomic availability/visibility operations.

## Directory Structure
- `src/main.cpp`: Memory model feature query, atomic buffer initialization, and compute dispatch host app.
- `CMakeLists.txt`: Build target configuration.
