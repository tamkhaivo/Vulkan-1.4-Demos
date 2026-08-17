# Assignment 83 – Dynamic Fragment Density Maps & Eye-Tracked Foveated Shading

## Overview & Architectural Critique
High-resolution 4K and XR displays strain GPU fragment fill-rates on peripheral geometry where human visual acuity is minimal. Variable Rate Shading (VRS) combined with dynamic foveation maps relaxes shading rates from $1\times 1$ to $4\times 4$ depending on fixation point coordinates. **Assignment 83** implements dynamic attachment shading rate maps using `VK_KHR_fragment_shading_rate` with dynamic gaze coordinates.

## Key Vulkan 1.4 Concepts
- **Attachment-Driven VRS**: Shading rate density maps.
- **Dynamic Foveated Shading**: Concentric shading rate rings ($1\times 1$, $2\times 2$, $4\times 4$).
- **Dynamic Rendering**: Dynamic viewport and foveated rate rendering.

## Acceptance Criteria
- [x] Configure fragment shading rate feature structures.
- [x] Render animated 3D geometry with dynamic foveation rate rings.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Foveated VRS pipeline runtime.
- `shaders/foveated_vrs.vert`, `shaders/foveated_vrs.frag`: Foveated VRS shaders.
- `CMakeLists.txt`: Build target configuration.
