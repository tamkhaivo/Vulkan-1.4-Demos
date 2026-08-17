# Assignment 69 – Hardware Acceleration Structure Serialization, Deserialization & Compaction (`VK_KHR_ray_tracing_pipeline`)

## Overview
Implement a high-performance BVH acceleration structure compaction and disk serialization pipeline. Query post-build compacted sizes (`VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR`), compact sparse BLAS structures for 40%+ memory savings, and serialize pre-built BVHs to persistent storage for instant reload.

## Key Concepts
- `vkCmdWriteAccelerationStructuresPropertiesKHR` with compacted size queries.
- `vkCmdCopyAccelerationStructureKHR` with `VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR`.
- BVH serialization (`VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR`) to host memory / disk.
- BVH deserialization (`VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR`) and pointer patching.
- Memory optimization and instant-load acceleration structure caching.

## Acceptance Criteria
- [x] Build initial uncompacted bottom-level acceleration structures (BLAS) for dense 3D meshes.
- [x] Execute query pools to extract post-build compacted BVH memory sizes.
- [x] Allocate compacted acceleration structure memory and execute `vkCmdCopyAccelerationStructureKHR`.
- [x] Serialize the compacted BVH to binary disk storage and deserialize into a new acceleration structure handle.
- [x] Trace rays against the deserialized and compacted BVH, validating identical visual output and reduced memory footprint.

## Directory Structure
- `src/main.cpp`: Acceleration structure compaction and serialization host application.
- `CMakeLists.txt`: Build target configuration.
