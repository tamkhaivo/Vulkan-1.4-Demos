# Assignment 39 – Displacement Micromaps & Micro-Mesh Ray Tracing (`VK_NV_displacement_micromap`)

## Overview & Architectural Critique
Representing micro-geometric displacement (such as rocky terrain, fabric creases, or skin wrinkles) in standard ray tracing requires tessellating triangles upfront, inflating BVH build times and consuming gigabytes of VRAM.

In Vulkan 1.4, **Displacement Micromaps (DMM, `VK_NV_displacement_micromap`)** allows ray tracing hardware to evaluate sub-triangle scalar displacement fields directly inside the acceleration structure traversal pipeline. The BLAS embeds base triangles paired with `VkMicromapNV` displacement arrays, achieving micro-mesh fidelity with up to 90% memory reduction and zero Any-Hit shader execution costs.

## Key Vulkan 1.4 Concepts
- **`VK_NV_displacement_micromap` Feature**: `displacementMicromap = VK_TRUE`.
- **`VkMicromapNV`**: Displacement micromap object holding scalar height offsets.
- **Micro-Mesh BLAS Attachment**: `VkAccelerationStructureTrianglesDisplacementMicromapNV` linked in `VkAccelerationStructureGeometryTrianglesDataKHR.pNext`.
- **Sub-Triangle Ray Traversal**: Hardware evaluates displaced micro-triangles during BVH intersection.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Displacement Micromap (DMM) Build Info
VkMicromapBuildInfoEXT dmmBuildInfo{
    .sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT,
    .type = VK_MICROMAP_TYPE_DISPLACEMENT_MICROMAP_NV,
    .flags = VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT,
    .mode = VK_BUILD_MICROMAP_MODE_BUILD_EXT,
    .dstMicromap = displacementMicromapObject,
    .usageCountsCount = 1,
    .pUsageCounts = &dmmUsageCount,
    .data = { .deviceAddress = dmmValuesDeviceAddress },
    .scratchData = { .deviceAddress = dmmScratchDeviceAddress },
    .triangleArray = { .deviceAddress = dmmTrianglesDeviceAddress },
    .triangleArrayStride = sizeof(VkMicromapTriangleEXT)
};
vkCmdBuildMicromapsEXT(cmd, 1, &dmmBuildInfo);

// 2. Attach DMM to BLAS Geometry Triangles Data
VkAccelerationStructureTrianglesDisplacementMicromapNV dmmAttachment{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_DISPLACEMENT_MICROMAP_NV,
    .micromap = displacementMicromapObject,
    .pUsageCounts = &dmmUsageCount,
    .usageCountsCount = 1,
    .data = { .deviceAddress = dmmValuesDeviceAddress },
    .triangleArray = { .deviceAddress = dmmTrianglesDeviceAddress },
    .triangleArrayStride = sizeof(VkMicromapTriangleEXT)
};
// Attached into trianglesData.pNext during vkCmdBuildAccelerationStructuresKHR
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_displacement_micromap` physical device features.
- [x] Build displacement micromap structures encoding sub-triangle height maps.
- [x] Link displacement micromap to BLAS geometry structure.
- [x] Ray trace micro-mesh displaced geometry and verify hardware intersection fidelity.
- [x] Confirm significant VRAM reduction compared to upfront dense triangle subdivision.

## Directory Structure
- `src/main.cpp`: Displacement micromaps host application.
- `shaders/dmm_raygen.rgen`, `shaders/dmm_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
