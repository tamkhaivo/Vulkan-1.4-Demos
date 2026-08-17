# Assignment 62 – Shader Core Builtins & Subgroup Cluster Operations (`VK_KHR_shader_subgroup_clustered`)

## Overview
Master intra-wave execution partitioning and cluster arithmetic operations using subgroup clustered intrinsics. Implement hierarchical parallel prefix scans, localized reductions across hardware SIMD clusters (e.g. 4, 8, 16 lanes), and evaluate fine-grained parallel reduction algorithms.

## Key Concepts
- Subgroup cluster operations: `subgroupClusteredAdd()`, `subgroupClusteredMul()`, `subgroupClusteredMin()`, `subgroupClusteredMax()`.
- Intra-warp/wave clustering with cluster sizes $K \in \{1, 2, 4, 8, 16, 32, 64\}$.
- Hierarchical parallel scan and segmented prefix sum algorithms without shared memory bank conflicts.
- Querying `subgroupSupportedOperations` (`VK_SUBGROUP_FEATURE_CLUSTERED_BIT`).

## Acceptance Criteria
- [x] Query physical device properties for `VK_SUBGROUP_FEATURE_CLUSTERED_BIT` and minimum/maximum subgroup sizes.
- [x] Write compute shaders performing localized spatial cluster reductions across 4-lane and 16-lane groups.
- [x] Implement a multi-level parallel segmented prefix scan entirely using clustered subgroup instructions.
- [x] Verify arithmetic output accuracy against ground truth CPU parallel scan results.
- [x] Ensure full SPIR-V validation compliance for Vulkan 1.4 subgroup cluster capabilities.

## Directory Structure
- `src/main.cpp`: Subgroup cluster host application.
- `CMakeLists.txt`: Build target configuration.
