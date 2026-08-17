# Assignment 25 – Clustered Forward 3D Tile Lighting & Workgroup Compute

## Overview
Render complex scenes with 1,000+ dynamic point lights in a single forward pass by slicing the view frustum into 3D AABB clusters and culling lights in compute workgroups with shared memory.

## Key Concepts
- 3D View-Frustum Clustering: slicing screen space (X, Y) and exponential depth (Z) into a 3D grid.
- Compute shader cluster AABB generation and light-sphere intersection testing.
- Workgroup shared memory light indexing and atomic counter compaction.
- Clustered forward fragment shader looking up active light lists per cluster index.

## Acceptance Criteria
- [x] Construct SSBOs for cluster bounding boxes, light data (positions, colors, radius), and cluster light index grids.
- [x] Dispatch compute shader to generate view-frustum cluster AABBs for current camera projection.
- [x] Dispatch light culling compute shader using workgroup shared memory to assign lights to clusters.
- [x] Synchronize compute culling buffer writes with fragment shader reads via `VkPipelineBarrier2`.
- [x] Evaluate dynamic clustered lighting in the forward fragment shader.

## Directory Structure
- `src/main.cpp`: Clustered forward lighting host application.
- `shaders/`: Cluster generation (`cluster_bounds.comp`), light culling (`cluster_cull.comp`), forward lighting (`forward.frag`).
- `CMakeLists.txt`: Build target configuration.
