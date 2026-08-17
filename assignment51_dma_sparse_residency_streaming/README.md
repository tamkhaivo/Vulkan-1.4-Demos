# Assignment 51 – Direct Linear DMA Staging & Sparse Residency (`vkQueueBindSparse`)

## Overview & Architectural Critique
Loading high-resolution volumetric datasets or massive multi-gigabyte texture arrays requires managing sparse memory page residency concurrently with background DMA streaming without hitching the main rendering pipeline.

In Vulkan 1.4, **Direct Linear DMA Staging with Sparse Residency** couples dedicated DMA copy queues with `vkQueueBindSparse`. As the camera navigates through the world, residency tracking compute kernels identify missing $64\text{KB}$ tiles, which are asynchronously staged via direct linear DMA transfers and committed into the sparse page tables without stalling the active frame rendering.

## Key Vulkan 1.4 Concepts
- **Sparse Virtual Memory Commitments**: Dynamic binding of physical `VkDeviceMemory` chunks via `vkQueueBindSparse`.
- **Asynchronous DMA Queue Transfer**: Direct host-to-device memory staging on dedicated transfer queue families.
- **Cross-Queue Timeline Synchronization**: Signaling completion to sparse binding queues using timeline semaphores.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Asynchronously bind committed physical memory pages
VkSparseMemoryBind sparseBind{
    .resourceOffset = pageIndex * SPARSE_PAGE_SIZE,
    .size = SPARSE_PAGE_SIZE,
    .memory = committedDeviceMemory,
    .memoryOffset = memoryPoolOffset,
    .flags = 0
};

VkSparseBufferMemoryBindInfo bufferBindInfo{
    .buffer = sparseBuffer,
    .bindCount = 1,
    .pBinds = &sparseBind
};

VkBindSparseInfo bindSparseInfo{
    .sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
    .bufferBindCount = 1,
    .pBufferBinds = &bufferBindInfo
};

vkQueueBindSparse(sparseQueue, 1, &bindSparseInfo, sparseFence);
```

## Acceptance Criteria
- [x] Create sparse buffer/image structures with `VK_BUFFER_CREATE_SPARSE_BINDING_BIT`.
- [x] Stream tile data asynchronously across dedicated DMA transfer queues.
- [x] Update sparse memory page table commitments via `vkQueueBindSparse`.
- [x] Render scene accessing dynamically committed sparse memory with 100% clean validation output.

## Directory Structure
- `src/main.cpp`: Direct DMA sparse residency host application.
- `shaders/sparse_stream.vert`, `shaders/sparse_stream.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
