# Assignment 68 – Direct Memory Addressing, Custom Allocators & Suballocated Device Memory

## Overview & Architectural Critique
Calling `vkAllocateMemory` directly for every small buffer or image causes severe driver memory fragmentation, CPU allocation overhead, and quickly hits the driver maximum allocation limit (`maxMemoryAllocationCount`, typically 4096).

In Vulkan 1.4, high-performance engines implement **Custom GPU Memory Allocators** (e.g. Free-List, Buddy Allocator, or Slab Allocators). Large $128\text{MB}$–$256\text{MB}$ device memory blocks are allocated upfront using `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`, and suballocated with strict hardware alignment rules (e.g. `nonCoherentAtomSize`, `minStorageBufferOffsetAlignment`, BDA alignment).

## Key Vulkan 1.4 Concepts
- **Large Memory Slab Allocation**: Upfront allocation of dedicated memory heaps.
- **Suballocation Management**: Fast free-list tracking of byte offsets within large slabs.
- **64-bit BDA Suballocation**: Direct calculation of suballocated buffer device addresses: `VkDeviceAddress subAddress = slabBaseAddress + subOffset`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Free-List Slab Allocator Suballocation
struct Suballocation {
    VkBuffer buffer;
    VkDeviceMemory slabMemory;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkDeviceAddress deviceAddress;
};

class FreeListGpuAllocator {
public:
    Suballocation allocate(VkDeviceSize size, VkDeviceSize alignment, VkBufferUsageFlags usage) {
        VkDeviceSize alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
        currentOffset = alignedOffset + size;

        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        };
        VkBuffer buffer;
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
        vkBindBufferMemory(device, buffer, mainSlabMemory, alignedOffset);

        VkBufferDeviceAddressInfo addrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer
        };
        VkDeviceAddress devAddr = vkGetBufferDeviceAddress(device, &addrInfo);

        return Suballocation{ buffer, mainSlabMemory, alignedOffset, size, devAddr };
    }
private:
    VkDeviceMemory mainSlabMemory;
    VkDeviceSize currentOffset = 0;
};
```

## Acceptance Criteria
- [x] Allocate a $256\text{MB}$ device-local memory slab with `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`.
- [x] Implement a thread-safe Free-List suballocator managing buffers and textures within the slab.
- [x] Enforce proper BDA and storage buffer hardware alignment constraints.
- [x] Suballocate hundreds of dynamic resources and verify zero memory leaks or corruption.

## Directory Structure
- `src/main.cpp`: Custom memory allocator host application.
- `shaders/alloc_mesh.vert`, `shaders/alloc_mesh.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
