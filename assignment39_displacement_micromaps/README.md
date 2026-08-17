# Assignment 39 – Displacement Micromaps & Neural Micro-Mesh Ray Tracing (`VK_NV_displacement_micromap`)

## Overview
Render ultra-detailed geometric detail and sub-triangle micro-displacement on ray-traced geometry without expanding raw triangle vertex buffers by embedding displacement micromaps (DMM) into the hardware BVH builder.

## Key Concepts
- `VK_NV_displacement_micromap` extension queries and format capabilities.
- Micro-mesh subdivision levels, direction vectors, and compressed format encodings (`VK_DISPLACEMENT_MICROMAP_FORMAT_64_TRIANGLES_64_BYTES_NV`).
- Linking `VkMicromapNV` displacement arrays with `VkAccelerationStructureGeometryTrianglesDataKHR`.
- Constructing base low-poly geometry BLAS with hardware-evaluated displacement on the fly.
- Extreme polygon density reduction in CPU/GPU VRAM with zero Any-Hit shader cost.

## Acceptance Criteria
- [x] Query micromap device limits and create a dedicated `VkMicromapNV` object for displacement data.
- [x] Encode displacement vector arrays and build the micromap using `vkCmdBuildMicromapsNV`.
- [x] Attach micromap information into the BLAS build descriptors (`VkAccelerationStructureTrianglesDisplacementMicromapNV`).
- [x] Build TLAS and trace rays against the micro-displaced surface.
- [x] Display high-frequency procedural geometric detail on low-poly base geometry with native hardware intersection performance.

## Directory Structure
- `src/main.cpp`: Displacement micromap creation, BLAS integration, and ray tracing host application.
- `CMakeLists.txt`: Build target configuration.
