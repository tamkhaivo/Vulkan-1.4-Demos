# Assignment 32 – Hardware Ray Tracing Opacity Micromaps (`VK_EXT_opacity_micromap`)

## Overview & Architectural Critique
Ray tracing foliage, chainlink fences, and hair traditionally requires executing expensive **Any-Hit shaders** (`closesthit/anyhit`) to test alpha cutout masks. This causes warp divergence and completely disables hardware traversal speedups.

In Vulkan 1.4, **Opacity Micromaps (OMM, `VK_EXT_opacity_micromap`)** embeds micro-opacity arrays (1-state, 2-state, or 4-state opacity per micro-triangle) directly into the Bottom-Level Acceleration Structure (BLAS). The hardware ray-tracing cores evaluate opacity during BVH traversal without invoking the Any-Hit shader, accelerating alpha-tested ray throughput by 2x–5x.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_opacity_micromap` Features**: `opacityMicromap = VK_TRUE`.
- **`VkMicromapEXT`**: Hardware micromap object built from raw opacity mask buffers.
- **BLAS Attachment**: `VkAccelerationStructureTrianglesOpacityMicromapEXT` attached to `VkAccelerationStructureGeometryTrianglesDataKHR.pNext`.
- **Micro-Triangle Resolution**: Setting `subdivisionLevel` to generate up to 1024 micro-triangles per base triangle.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Build Opacity Micromap (OMM)
VkMicromapBuildInfoEXT ommBuildInfo{
    .sType = VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT,
    .type = VK_MICROMAP_TYPE_OPACITY_MICROMAP_EXT,
    .flags = VK_BUILD_MICROMAP_PREFER_FAST_TRACE_BIT_EXT,
    .mode = VK_BUILD_MICROMAP_MODE_BUILD_EXT,
    .dstMicromap = micromapObject,
    .usageCountsCount = 1,
    .pUsageCounts = &ommUsageCount,
    .data = { .deviceAddress = ommDataDeviceAddress },
    .scratchData = { .deviceAddress = ommScratchDeviceAddress },
    .triangleArray = { .deviceAddress = ommTriangleIndicesDeviceAddress },
    .triangleArrayStride = sizeof(VkMicromapTriangleEXT)
};
vkCmdBuildMicromapsEXT(cmd, 1, &ommBuildInfo);

// 2. Attach OMM to BLAS Geometry Triangles Data
VkAccelerationStructureTrianglesOpacityMicromapEXT ommAttachment{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT,
    .micromap = micromapObject,
    .pUsageCounts = &ommUsageCount,
    .usageCountsCount = 1,
    .data = { .deviceAddress = ommDataDeviceAddress },
    .triangleArray = { .deviceAddress = ommTriangleIndicesDeviceAddress },
    .triangleArrayStride = sizeof(VkMicromapTriangleEXT)
};

VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
    .pNext = &ommAttachment, // Hardware OMM attached directly to BLAS build
    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
    .vertexData = { .deviceAddress = vertexBufferAddress },
    .vertexStride = sizeof(Vertex),
    .maxVertex = vertexCount,
    .indexType = VK_INDEX_TYPE_UINT32,
    .indexData = { .deviceAddress = indexBufferAddress }
};
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_opacity_micromap` features on the physical device.
- [x] Bake 2-state / 4-state opacity micromap buffers for alpha-tested leaf/foliage textures.
- [x] Build `VkMicromapEXT` object and attach to BLAS geometry via `VkAccelerationStructureTrianglesOpacityMicromapEXT`.
- [x] Ray trace alpha-tested foliage scene with `gl_RayFlagsOpaqueEXT` and verify zero Any-Hit shader execution stalls.
- [x] Measure and log ray traversal performance speedup compared to standard Any-Hit alpha testing.

## Directory Structure
- `src/main.cpp`: Opacity micromaps host application.
- `shaders/omm_raygen.rgen`, `shaders/omm_miss.rmiss`, `shaders/omm_hit.rchit`: RT shaders.
- `CMakeLists.txt`: Build target configuration.
