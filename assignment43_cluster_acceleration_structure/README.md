# Assignment 43 – Ray Tracing Partitioned Clusters & BVH Compaction (`VK_NV_cluster_acceleration_structure`)

## Overview
Build next-generation fine-grained hierarchical acceleration structures using Partitioned Cluster Acceleration Structures (CLAS) to accelerate geometry streaming, dynamic LOD hierarchies, and real-time streaming BVH updates.

## Key Concepts
- `VK_NV_cluster_acceleration_structure` feature and architecture.
- Cluster-level acceleration structures (CLAS) underneath BLAS / TLAS trees.
- Implicit cluster bounding volume hierarchies for Nanite-style dynamic meshlet geometry.
- BVH compaction and fast GPU-side rebuilds for streaming high-poly assets.
- Acceleration structure memory budget management and defragmentation.

## Acceptance Criteria
- [x] Query and enable `clusterAccelerationStructure` device features.
- [x] Partition a high-density triangle mesh into clusters/meshlets on the GPU.
- [x] Build Cluster Acceleration Structures (CLAS) and link them into parent BLAS nodes via `vkCmdBuildClusterAccelerationStructureNV`.
- [x] Execute hardware ray queries / ray tracing against the clustered acceleration structure.
- [x] Demonstrate GPU-driven continuous level-of-detail (LOD) ray tracing with minimal BVH rebuild times.

## Directory Structure
- `src/main.cpp`: Cluster acceleration structure building, BLAS linking, and ray query execution host application.
- `CMakeLists.txt`: Build target configuration.
