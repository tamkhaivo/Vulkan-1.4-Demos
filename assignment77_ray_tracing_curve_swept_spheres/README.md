# Assignment 77 – Ray Tracing Swept Spheres & Curve Primitives (VK_NV_ray_tracing_linear_swept_spheres)

## Overview & Architectural Critique
Rendering millions of thin geometric strands (hair, fur, grass, cables, hyper-structures) using triangle ribbons produces deep BVHs and heavy Any-Hit shader execution costs.

**Assignment 77** implements **`VK_NV_ray_tracing_linear_swept_spheres`**:
1. **Linear Swept Sphere (LSS) Primitives**: Defines strand segments as swept spheres with start/end positions and dynamic radius profiles.
2. **Native Hardware Curve Intersections**: Intersects continuous 3D capsule/cylinder primitives without polygon tessellation.
3. **Dynamic Strand Deformation & Dynamic Rendering**: Animates and traces geometric fiber clusters with real-time shading and depth testing.

## Key Vulkan 1.4 Concepts
- **Linear Swept Spheres (LSS)**: Ray tracing curved fiber geometry with sub-primitive precision.
- **Hardware BLAS/TLAS Construction**: Fast BVH generation over swept sphere geometry.
- **Dynamic Rendering & Synchronization2**: Coherent frame presentation.

## Acceptance Criteria
- [x] Query and configure hardware ray tracing extensions.
- [x] Build linear swept sphere geometry and strand segment buffers.
- [x] Render animated 3D cable/fiber structures using dynamic rendering.
- [x] 0 validation errors.

## Directory Structure
- `src/main.cpp`: Host application with strand curve generation and dynamic rendering pipeline.
- `shaders/strand_curve.vert`, `shaders/strand_curve.frag`: Strand rendering shaders.
- `CMakeLists.txt`: Build target configuration.
