# Assignment 33 – Subgroup Advanced Partitioning & Quad Operations (`VK_NV_shader_subgroup_partitioned` / Subgroup Quad)

## Overview & Architectural Critique
When grouping and sorting dynamic GPU elements (such as material bins, light bins, or ray hits), traditional methods rely on global atomics or shared memory sorting passes that stall SIMD lanes.

In Vulkan 1.4, **Subgroup Partitioning (`VK_NV_shader_subgroup_partitioned`)** and **Subgroup Quad Operations** allow threads within a wave to partition themselves based on shared keys (`subgroupPartitionNV()`), creating dynamic sub-clusters on-chip without synchronization barriers. Subgroup quad operations (`subgroupQuadBroadcast`, `subgroupQuadSwapHorizontal`) enable analytic screen-space derivatives ($2\times 2$ pixel footprints) directly in compute shaders.

## Key Vulkan 1.4 Concepts
- **Subgroup Partitioning**: `uvec4 mask = subgroupPartitionNV(key)` partitioning threads with identical keys.
- **Quad Operations**: `subgroupQuadBroadcast(val, id)` and `subgroupQuadSwapDiagonal(val)`.
- **Lock-Free Binning**: Parallel atomic bin compaction inside warp registers.

## Concrete Implementation Example (GLSL Compute Shader)

```glsl
#version 460
#extension GL_NV_shader_subgroup_partitioned : require
#extension GL_KHR_shader_subgroup_quad : require
#extension GL_KHR_shader_subgroup_ballot : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) readonly buffer KeysBuffer {
    uint materialKeys[];
};

layout(std430, set = 0, binding = 1) buffer HistogramBuffer {
    uint binCounts[16];
};

void main() {
    uint globalId = gl_GlobalInvocationID.x;
    uint myKey = materialKeys[globalId] & 0xF; // 16 possible material bins

    // 1. Partition threads with matching keys in a single instruction
    uvec4 partitionMask = subgroupPartitionNV(myKey);
    uint numMatchingThreads = subgroupBallotBitCount(partitionMask);
    uint mySubIndex = subgroupBallotExclusiveBitCount(partitionMask);

    // 2. Only the first thread in each partition updates the global bin atomic
    if (mySubIndex == 0) {
        atomicAdd(binCounts[myKey], numMatchingThreads);
    }
}
```

## Acceptance Criteria
- [x] Query physical device subgroup features to verify partitioned and quad support.
- [x] Implement compute shader using `subgroupPartitionNV()` for lock-free binning across 100,000+ elements.
- [x] Implement compute image filter computing screen-space gradients via `subgroupQuadSwapHorizontal` / `Vertical`.
- [x] Verify mathematical correctness of bin counts and clean validation layer output.

## Directory Structure
- `src/main.cpp`: Subgroup partitioned and quad operations host application.
- `shaders/subgroup_partition.comp`, `shaders/quad_filter.comp`: Shaders.
- `CMakeLists.txt`: Build target configuration.
