# Assignment 43 – Ray Tracing Partitioned Clusters & BVH Compaction (`VK_NV_cluster_acceleration_structure`)

## Overview & Architectural Critique
When ray tracing massive geometric scenes with continuous Nanite-style dynamic Level of Detail (LOD), rebuilding full Bottom-Level Acceleration Structures (BLAS) per frame on the GPU causes massive compute stalls ($10\text{ms}+$ per frame).

In Vulkan 1.4, **Cluster-Level Acceleration Structures (CLAS, `VK_NV_cluster_acceleration_structure`)** introduces a sub-BLAS hierarchical acceleration tier. Individual geometry clusters (e.g. 64-128 triangles) have their BVHs built and compacted independently in GPU memory, allowing the engine to rebuild only modified dynamic clusters in sub-millisecond compute passes.

## Key Vulkan 1.4 Concepts
- **`VK_NV_cluster_acceleration_structure` Feature**: `clusterAccelerationStructure = VK_TRUE`.
- **Hierarchical AS Layout**: Top-Level (TLAS) -> Bottom-Level (BLAS) -> Cluster-Level (CLAS).
- **Cluster BVH Rebuilds**: Rebuilding only modified cluster nodes without invalidating the top-level scene BVH.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Cluster Acceleration Structure Build Info
VkClusterAccelerationStructureCommandsInfoNV clusterCommandsInfo{
    .sType = VK_STRUCTURE_TYPE_CLUSTER_ACCELERATION_STRUCTURE_COMMANDS_INFO_NV,
    .input = {
        .type = VK_CLUSTER_ACCELERATION_STRUCTURE_TYPE_CLUSTERS_BOTTOM_LEVEL_NV,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .opType = VK_CLUSTER_ACCELERATION_STRUCTURE_OP_TYPE_BUILD_CLUSTERS_BOTTOM_LEVEL_NV,
        .opData = { .pClustersBottomLevel = &clustersBottomLevelInput }
    },
    .dstAddressesArray = { .deviceAddress = dstAddressArrayDeviceAddress },
    .dstSizesArray = { .deviceAddress = dstSizesArrayDeviceAddress },
    .scratchData = { .deviceAddress = clusterScratchDeviceAddress }
};

// 2. Build CLAS on GPU Command Buffer
vkCmdBuildClusterAccelerationStructureNV(cmd, &clusterCommandsInfo);
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_cluster_acceleration_structure` features on physical device.
- [x] Partition complex geometry into independent triangle clusters.
- [x] Build CLAS structures on the GPU using `vkCmdBuildClusterAccelerationStructureNV`.
- [x] Ray trace through the cluster BVH hierarchy in real-time.
- [x] Demonstrate microsecond-level dynamic cluster rebuilds under animated geometry.

## Directory Structure
- `src/main.cpp`: Cluster acceleration structure host application.
- `shaders/clas_raygen.rgen`, `shaders/clas_closesthit.rchit`: Shaders.
- `CMakeLists.txt`: Build target configuration.
