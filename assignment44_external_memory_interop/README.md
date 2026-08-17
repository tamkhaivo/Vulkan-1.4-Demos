# Assignment 44 – External Memory Interop & CUDA/Direct3D 12 Synchronization (`VK_KHR_external_memory_win32`)

## Overview & Architectural Critique
Heterogeneous computing pipelines often require executing AI inference in CUDA, physics simulation in Direct3D 12, and high-performance rasterization/ray tracing in Vulkan. Copying resources through host CPU system RAM introduces prohibitive PCIe bandwidth stalls.

In Vulkan 1.4, **External Memory & Semaphore Interop (`VK_KHR_external_memory_win32`, `VK_KHR_external_semaphore_win32`)** enables zero-copy resource sharing directly in GPU VRAM across graphics APIs and compute frameworks using Windows NT Handles (`HANDLE`) or POSIX file descriptors (`int fd`).

## Key Vulkan 1.4 Concepts
- **Exporting External Memory**: `VkExportMemoryAllocateInfo` specifying `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT`.
- **Exporting External Semaphores**: `VkExportSemaphoreCreateInfo` specifying `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT`.
- **Querying Windows NT Handles**: `vkGetMemoryWin32HandleKHR` and `vkGetSemaphoreWin32HandleKHR`.
- **Cross-API Timeline Signaling**: Direct synchronization between CUDA streams and Vulkan queues without CPU locks.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Exportable Memory Allocation Info
VkExportMemoryAllocateInfo exportAllocInfo{
    .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
};

VkMemoryAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = &exportAllocInfo,
    .allocationSize = memRequirements.size,
    .memoryTypeIndex = memTypeIndex
};
vkAllocateMemory(device, &allocInfo, nullptr, &sharedDeviceMemory);

// 2. Extract Windows NT Handle to share with CUDA / D3D12
VkMemoryGetWin32HandleInfoKHR getHandleInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
    .memory = sharedDeviceMemory,
    .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
};
HANDLE sharedMemoryHandle = NULL;
vkGetMemoryWin32HandleKHR(device, &getHandleInfo, &sharedMemoryHandle);

// (The handle 'sharedMemoryHandle' can now be imported directly into CUDA via cudaImportExternalMemory)
```

## Acceptance Criteria
- [x] Query external memory and semaphore capabilities on physical device.
- [x] Allocate device memory with `VkExportMemoryAllocateInfo`.
- [x] Retrieve valid Windows NT shared handle via `vkGetMemoryWin32HandleKHR`.
- [x] Create exportable timeline semaphore for cross-engine lock-free synchronization.
- [x] Demonstrate zero-copy resource consumption with clean validation layer execution.

## Directory Structure
- `src/main.cpp`: External memory interop host application.
- `shaders/interop_scene.vert`, `shaders/interop_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
