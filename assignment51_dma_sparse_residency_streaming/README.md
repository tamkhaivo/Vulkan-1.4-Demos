# Assignment 51 – Direct Linear DMA Staging with Sparse Residency & VMA

## Overview
Architect a high-performance texture/geometry streaming engine capable of handling gigabyte-scale sparse virtual assets using `vkQueueBindSparse`, physical $64\text{KB}$ page table commits, and non-blocking background streaming queues.

## Key Concepts
- Sparse image residency with `vkQueueBindSparse` and `VkSparseMemoryBind`.
- Dynamic page resident tracking with quadtree / octree LOD visibility calculations.
- Direct host-to-device asynchronous streaming queues (`VK_QUEUE_TRANSFER_BIT`).
- Tile residency tracking texture and base mip fallbacks.

## Acceptance Criteria
- [x] Query sparse image properties and tile granularity requirements.
- [x] Allocate a partially resident sparse 2D image.
- [x] Stream memory tiles dynamically via background transfer queue.
- [x] Bind resident pages using `vkQueueBindSparse` synchronized via timeline semaphores.
- [x] Validate zero CPU hitches during streaming transitions.

## Directory Structure
- `src/main.cpp`: Sparse residency allocator and streaming controller.
- `CMakeLists.txt`: Build target configuration.
