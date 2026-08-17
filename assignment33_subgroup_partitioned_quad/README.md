# Assignment 33 – Subgroup Advanced Partitioning & Quad Operations (`VK_NV_shader_subgroup_partitioned` / Subgroup Quad)

## Overview
Implement advanced wave-level data binning with `subgroupPartitionNV()` and compute analytic screen-space derivatives using subgroup quad shuffle operations without helper invocation overhead.

## Key Concepts
- Subgroup Quad operations: `subgroupQuadBroadcast`, `subgroupQuadSwapHorizontal`, `subgroupQuadSwapVertical`, `subgroupQuadSwapDiagonal`.
- Subgroup Partitioned operations: `subgroupPartitionNV()` generating bitmasks of SIMD lanes sharing identical keys.
- Lock-free, collision-free GPU histogram/binning without atomics or shared memory locks.
- Screen-space 2x2 quad derivative calculations in compute shaders.

## Acceptance Criteria
- [x] Enable `VK_NV_shader_subgroup_partitioned` and subgroup quad operations.
- [x] Implement compute shader utilizing `subgroupPartitionNV()` to partition workgroup data by classification keys.
- [x] Utilize `subgroupQuadSwapHorizontal` and `subgroupQuadSwapVertical` to compute finite differences across 2x2 pixel quads.
- [x] Verify arithmetic correctness of lock-free parallel binning against CPU reference calculations.
- [x] Output processed results cleanly.

## Directory Structure
- `src/main.cpp`: Subgroup partitioned and quad host application.
- `shaders/`: Compute shader (`subgroup_partition.comp`).
- `CMakeLists.txt`: Build target configuration.
