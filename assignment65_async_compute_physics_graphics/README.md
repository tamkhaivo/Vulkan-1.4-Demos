# Assignment 65 – Multi-Queue Direct Compute Physics & Graphics Pipeline

## Overview
Design and execute a decoupled asynchronous simulation and rendering loop. Run an intensive particle / rigid-body compute physics simulation on a dedicated `VK_QUEUE_COMPUTE_BIT` queue, continuously streaming updated state to a concurrent `VK_QUEUE_GRAPHICS_BIT` rendering pipeline synchronized exclusively via 64-bit timeline semaphores.

## Key Concepts
- Multi-queue physical device discovery (`computeQueueFamilyIndex != graphicsQueueFamilyIndex`).
- Lock-free dual-buffered physics state exchange.
- Timeline semaphore monotonic dependency chaining (`vkQueueSubmit2` with `VkTimelineSemaphoreSubmitInfo`).
- Fine-grained buffer ownership release/acquire barriers across asynchronous queue boundaries.

## Acceptance Criteria
- [x] Discover and initialize distinct hardware Graphics and Compute queue families.
- [x] Allocate ping-pong storage buffers configured for concurrent or exclusive queue sharing.
- [x] Record and submit compute simulation passes on the compute queue using timeline semaphore signal values $N$.
- [x] Record graphics rendering passes waiting on timeline value $N$ and executing asynchronously with overlap.
- [x] Verify continuous non-stalling simulation and render loops without synchronization hazards.

## Directory Structure
- `src/main.cpp`: Multi-queue async compute physics host application.
- `CMakeLists.txt`: Build target configuration.
