# Assignment 84 – Clustered Level Acceleration Structures (CLAS) for Micro-Mesh BVH Compaction

## Overview & Architectural Critique
Traditional Bottom-Level Acceleration Structures (BLAS) rebuilds for procedural, deforming, or highly detailed micro-polygon meshes cause massive CPU/GPU synchronization bubbles. **Assignment 84** implements `VK_NV_cluster_acceleration_structure` principles, where meshes are grouped into independent clusters ($128$ to $512$ triangles) that can be individually pruned, updated, and compacted in sub-milliseconds without whole-BLAS invalidation.

## Key Vulkan 1.4 Concepts
- **Cluster Acceleration Structures (CLAS)**: Subdividing geometry into discrete clusters.
- **Dynamic Cluster Compaction**: Pruning back-facing/occluded clusters.
- **Dynamic Rendering**: Rendering animated cluster hierarchy geometry.

## Acceptance Criteria
- [x] Configure cluster partition structures and buffers.
- [x] Render animated micro-mesh clusters with dynamic LOD selection.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Cluster acceleration structure pipeline.
- `shaders/cluster_bvh.vert`, `shaders/cluster_bvh.frag`: Micro-mesh cluster shaders.
- `CMakeLists.txt`: Build target configuration.
