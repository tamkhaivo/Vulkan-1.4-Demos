# Assignment 68 – Direct Memory Addressing & Custom Suballocated GPU Memory (`VK_KHR_buffer_device_address`)

## Overview
Construct a production-grade custom GPU memory management architecture using suballocation blocks, buddy/free-list allocators, memory type categorization, and Vulkan 1.4 dedicated allocation hints, eliminating driver allocation bottlenecks and VRAM fragmentation.

## Key Concepts
- Suballocating large $128\text{MB}$ / $256\text{MB}$ `VkDeviceMemory` chunks.
- Memory property matching (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`, `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`).
- Dedicated allocation extension (`VK_KHR_dedicated_allocation` / Vulkan 1.4 Core).
- Buffer Device Address (BDA) offset calculation and alignment management.
- Lock-free memory pool management and fragmentation metrics.

## Acceptance Criteria
- [x] Implement a reusable Free-List / Buddy GPU Memory Allocator class.
- [x] Query `VkPhysicalDeviceMemoryProperties` and categorize heaps by budget, type, and speed.
- [x] Allocate large backing memory slabs and suballocate buffers, textures, and acceleration structures.
- [x] Verify zero memory leaks, proper alignment handling ($256$ bytes for UBOs, BDA alignments), and fast allocation benchmarks.
- [x] Pass all Vulkan validation memory tracking checks without error.

## Directory Structure
- `src/main.cpp`: GPU memory manager and suballocation host application.
- `CMakeLists.txt`: Build target configuration.
