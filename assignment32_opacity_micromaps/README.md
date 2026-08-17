# Assignment 32 – Hardware Ray Tracing Opacity Micromaps (OMM) (`VK_EXT_opacity_micromap`)

## Overview
Accelerate ray-traced alpha testing (e.g. foliage, fences, cutouts) by embedding hardware 2-state/4-state micro-opacity arrays directly into Bottom-Level Acceleration Structures (BLAS), eliminating expensive Any-Hit shader execution stalls.

## Key Concepts
- `VK_EXT_opacity_micromap` extension and device features.
- Opacity Micromap Array (`VkMicromapEXT`) creation and data packing (2-state: opaque/transparent, 4-state: unknown-opaque/unknown-transparent).
- Linking micromaps to BLAS triangle geometry descriptions (`VkAccelerationStructureTrianglesOpacityMicromapEXT`).
- Hardware ray traversal resolving hit status directly in BVH traversal units without shader invocation.

## Acceptance Criteria
- [x] Query and enable `VK_EXT_opacity_micromap` features on supported hardware.
- [x] Allocate and build a `VkMicromapEXT` object with packed opacity bits.
- [x] Attach micromap structure to triangle BLAS geometry during acceleration structure build.
- [x] Build TLAS referencing the BLAS.
- [x] Execute ray tracing workloads observing hardware-accelerated opacity filtering.

## Directory Structure
- `src/main.cpp`: Opacity micromap host application.
- `shaders/`: Ray tracing / ray query shaders.
- `CMakeLists.txt`: Build target configuration.
