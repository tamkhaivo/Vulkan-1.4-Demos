# Assignment 62 – Shader Core Builtins & Subgroup Cluster Operations (`VK_KHR_shader_subgroup_clustered`)

## Overview & Architectural Critique
While standard subgroup operations (`subgroupAdd`, `subgroupInclusiveAdd`) reduce across the entire warp (e.g. 32 or 64 lanes), many parallel algorithms (such as hierarchical segmented prefix scans, FFT butterflying, and $4\times 4$ matrix inversions) require localized reductions within sub-clusters of power-of-two sizes ($K \in \{2, 4, 8, 16\}$).

In Vulkan 1.4, **Subgroup Clustered Operations (`VK_KHR_shader_subgroup_clustered` / Vulkan 1.4 Core)** introduces `subgroupClusteredAdd`, `subgroupClusteredMin`, `subgroupClusteredMax`, and `subgroupClusteredMul`. These operations compute independent parallel reductions across intra-warp clusters with zero cross-cluster interference and zero shared memory overhead.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Clustered Subgroups**: `VK_SUBGROUP_FEATURE_CLUSTERED_BIT` in physical device properties.
- **GLSL Clustered Operations**: `#extension GL_KHR_shader_subgroup_clustered : require`.
- **Intra-Warp Cluster Reductions**: `subgroupClusteredAdd(val, clusterSize)` where `clusterSize` is a compile-time power-of-two constant.

## Concrete Implementation Example (GLSL Compute Shader)

```glsl
#version 460
#extension GL_KHR_shader_subgroup_clustered : require

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) readonly buffer InputBuffer {
    vec4 elements[];
};

layout(std430, set = 0, binding = 1) buffer OutputBuffer {
    vec4 clusteredSums[];
};

void main() {
    uint globalId = gl_GlobalInvocationID.x;
    vec4 myVal = elements[globalId];

    // Compute independent reductions within clusters of 4 threads (e.g. for 4x4 matrix/quad math)
    vec4 cluster4Sum = subgroupClusteredAdd(myVal, 4);

    // Compute independent reductions within clusters of 16 threads
    vec4 cluster16Sum = subgroupClusteredAdd(myVal, 16);

    clusteredSums[globalId] = cluster4Sum + cluster16Sum;
}
```

## Acceptance Criteria
- [x] Query and verify `VK_SUBGROUP_FEATURE_CLUSTERED_BIT` on physical device.
- [x] Implement compute shader utilizing `subgroupClusteredAdd` with cluster sizes 2, 4, 8, and 16.
- [x] Validate reduction accuracy against host CPU reference.
- [x] Confirm clean validation layer output and optimal wave occupancy.

## Directory Structure
- `src/main.cpp`: Subgroup cluster operations host application.
- `shaders/subgroup_cluster.comp`: GLSL compute shader.
- `CMakeLists.txt`: Build target configuration.
