# Assignment 69 – Hardware Acceleration Structure Serialization, Deserialization & Compaction (`VK_KHR_ray_tracing_pipeline`)

## Overview & Architectural Critique
Building hardware Bottom-Level Acceleration Structures (BLAS) for dense meshes produces conservative, uncompacted BVH memory allocations. In large game worlds, uncompacted BLAS memory can exceed VRAM capacity by 40%–50%. Furthermore, rebuilding static BLAS every time a level loads causes unnecessary CPU and GPU initialization stalls.

In Vulkan 1.4, **Acceleration Structure Compaction and Serialization** enables:
1. **Compacted Size Query**: Querying the actual post-build size via `VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR` and copying to a compacted BLAS (`VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR`).
2. **Disk Serialization**: Serializing compacted acceleration structures into binary disk files via `vkCmdCopyAccelerationStructureToMemoryKHR`, allowing fast deserialization (`vkCmdCopyMemoryToAccelerationStructureKHR`) on subsequent level loads.

## Key Vulkan 1.4 Concepts
- **Compaction Query Pool**: `vkCmdWriteAccelerationStructuresPropertiesKHR` with `VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR`.
- **Compacted Copy**: `vkCmdCopyAccelerationStructureKHR` with `VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR`.
- **Direct Memory Serialization**: `vkCmdCopyAccelerationStructureToMemoryKHR` writing binary BVHs into BDA storage buffers.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Query Post-Build Compacted Size
vkCmdWriteAccelerationStructuresPropertiesKHR(
    cmd,
    1,
    &uncompactedBlas,
    VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
    queryPool,
    0
);

// 2. Read Back Compacted Size and Allocate Compacted BLAS
VkDeviceSize compactedSize = 0;
vkGetQueryPoolResults(device, queryPool, 0, 1, sizeof(VkDeviceSize), &compactedSize, sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

VkAccelerationStructureCreateInfoKHR compactBlasInfo{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = compactedBlasBuffer,
    .size = compactedSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
};
vkCreateAccelerationStructureKHR(device, &compactBlasInfo, nullptr, &compactedBlas);

// 3. Execute GPU Compaction Copy
VkCopyAccelerationStructureInfoKHR copyInfo{
    .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
    .src = uncompactedBlas,
    .dst = compactedBlas,
    .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
};
vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);
```

## Acceptance Criteria
- [x] Query compacted size of built BLAS using `VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR`.
- [x] Allocate compacted acceleration structure and copy BVH using `VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR`.
- [x] Demonstrate 35%+ VRAM reduction on complex static geometry.
- [x] Serialize compacted BLAS to disk and reload via `vkCmdCopyMemoryToAccelerationStructureKHR`.
- [x] Verify ray tracing fidelity and 100% clean validation layer output.

## Directory Structure
- `src/main.cpp`: AS compaction and serialization host application.
- `shaders/compact_raygen.rgen`, `shaders/compact_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
