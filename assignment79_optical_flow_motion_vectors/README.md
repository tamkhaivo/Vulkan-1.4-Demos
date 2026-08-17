# Assignment 79 – Hardware Optical Flow & Motion Vector Estimation (VK_NV_optical_flow)

## Overview & Architectural Critique
Frame generation and temporal super-sampling algorithms (DLSS 3, FSR 3) rely on sub-pixel accurate 2D screen-space motion fields to interpolate frames.

**Assignment 79** implements **`VK_NV_optical_flow`**:
1. **Dedicated Hardware Optical Flow Acceleration**: Computes dense 2D velocity vectors across successive video/rendering frames.
2. **Dense Flow Vector Visualization & Extrapolation**: Evaluates and visualizes $S16.0$ fixed-point motion fields mapped to color-coded HSV vectors.
3. **Dynamic Rendering Integration**: Seamlessly integrates flow estimation passes into standard dynamic rendering color targets.

## Key Vulkan 1.4 Concepts
- **`VK_NV_optical_flow`**: Dedicated optical flow sessions and hardware queues.
- **Screen-Space Motion Vector Generation**: Per-pixel velocity buffers and direction fields.
- **Dynamic Rendering & Synchronization2**: Cross-frame feedback resource synchronization.

## Acceptance Criteria
- [x] Query and configure hardware optical flow / motion estimation features.
- [x] Generate screen-space motion vectors for dynamic spinning 3D objects.
- [x] Visualize motion fields using direction/magnitude color ramps with dynamic rendering.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Host application with motion field simulation and flow vector renderer.
- `shaders/flow_field.vert`, `shaders/flow_field.frag`: Flow visualization shaders.
- `CMakeLists.txt`: Build target configuration.
