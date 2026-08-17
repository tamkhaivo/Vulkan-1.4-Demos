# Assignment 44 – External Memory Interop & CUDA/Direct3D12 Synchronization (`VK_KHR_external_memory_win32` / `VK_KHR_external_semaphore_win32`)

## Overview
Achieve zero-copy interop between Vulkan 1.4 and external runtimes (CUDA / Direct3D 12 / DirectML) by sharing device memory allocations via Windows NT handles and synchronizing engines using shared timeline semaphores.

## Key Concepts
- `VK_KHR_external_memory`, `VK_KHR_external_memory_win32`, and `VK_KHR_external_semaphore_win32`.
- Exporting and importing `VkDeviceMemory` via Windows NT handles (`HANDLE`).
- Exporting Vulkan timeline semaphores to external Direct3D12/CUDA timeline semaphores (`D3D12_SHARED_RESOURCE_FLAG`).
- Synchronizing a Vulkan rendering engine with an external GPGPU simulation kernel without host round-trips.

## Acceptance Criteria
- [x] Allocate exportable `VkDeviceMemory` with `VkExportMemoryAllocateInfoKHR` and retrieve a Windows `HANDLE` via `vkGetMemoryWin32HandleKHR`.
- [x] Export a timeline `VkSemaphore` to a shareable Windows handle.
- [x] Demonstrate cross-API zero-copy sharing of a render target / compute buffer.
- [x] Coordinate execution using cross-engine semaphore signal and wait operations.
- [x] Render the shared buffer content in the Vulkan swapchain without CPU-side staging readbacks.

## Directory Structure
- `src/main.cpp`: External memory and semaphore export/import host application.
- `CMakeLists.txt`: Build target configuration.
