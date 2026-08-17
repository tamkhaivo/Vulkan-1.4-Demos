# Assignment 5 – Instanced Rendering with Vertex Attribute Divisor

## Overview
Render thousands of animated 3D mesh instances in a single indexed draw call by setting up multi-binding vertex input streams with per-instance attribute stepping using Vulkan 1.4 core vertex attribute divisor support.

## Key Concepts
- Instanced draw calls using `vkCmdDrawIndexed` with `instanceCount > 1`.
- Multi-binding vertex buffer input layouts (Binding 0: Per-vertex geometry data, Binding 1: Per-instance transform/color data).
- Vertex attribute divisor (`VkVertexInputBindingDivisorDescriptionKHR` / Vulkan 1.4 core `vertexAttributeDivisor`) with divisor value 1.
- `gl_InstanceIndex` and per-instance vertex shader attributes (`inInstancePos`, `inInstanceColor`, `inInstanceScale`).
- High-performance rendering of dense asteroid fields, grids, or particle meshes with zero CPU-draw-call overhead.

## Acceptance Criteria
- [x] Enable vertex attribute divisor feature in Vulkan 1.4 device initialization.
- [x] Create per-vertex buffer (mesh geometry) and per-instance buffer (position, rotation, color, scale).
- [x] Configure `VkPipelineVertexInputStateCreateInfo` with 2 binding descriptions and binding 1 set to `VK_VERTEX_INPUT_RATE_INSTANCE` with divisor 1.
- [x] Stream dynamic instance data to the instance buffer each frame (orbital rotations, pulsing scales).
- [x] Bind both vertex and instance buffers via `vkCmdBindVertexBuffers`.
- [x] Issue a single `vkCmdDrawIndexed` with thousands of instances rendered simultaneously.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`instanced.vert`, `instanced.frag`).
- `CMakeLists.txt`: Build target configuration.
