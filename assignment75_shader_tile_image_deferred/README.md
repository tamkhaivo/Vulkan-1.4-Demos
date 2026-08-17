# Assignment 75 – Tile-Local Subpass Operations & Dynamic Shading via Tile Image (VK_EXT_shader_tile_image)

## Overview & Architectural Critique
On Tile-Based Deferred Rendering (TBDR) GPUs (mobile silicon, Apple M-Series, modern integrated GPUs), flushing intermediate G-Buffer targets (Albedo, Normals, Depth) to system VRAM incurs immense power and memory bandwidth penalties.

**Assignment 75** implements **`VK_EXT_shader_tile_image`**:
1. **On-Chip SRAM Register Reads**: Fragment shaders read directly from color and depth tile registers without coordinate addressing or texture fetch latencies.
2. **Subpass-Free Deferred Shading**: Combines G-Buffer generation and multi-light deferred evaluation directly in on-chip tile memory.
3. **Zero-VRAM Bandwidth Consumption**: Eliminates external memory round-trips for intermediate lighting layers.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_shader_tile_image`**: `VkPhysicalDeviceShaderTileImageFeaturesEXT.shaderTileImageColorReadAccess`.
- **Tile-Image Fragment Declarations**: `tileImageReadEXT` accessing on-chip color registers.
- **Dynamic Rendering Local Read Integration**: Vulkan 1.4 dynamic rendering executing on-chip deferred shading.

## Acceptance Criteria
- [x] Query and configure `VK_EXT_shader_tile_image` extension.
- [x] Build multi-pass dynamic rendering deferred lighting pipeline reading local tile memory.
- [x] Render dynamic multi-light 3D scene with zero validation errors.

## Directory Structure
- `src/main.cpp`: Host application managing tile-local deferred passes.
- `shaders/gbuffer.vert`, `shaders/gbuffer.frag`, `shaders/tile_deferred.frag`: Shader suite.
- `CMakeLists.txt`: Build target configuration.
