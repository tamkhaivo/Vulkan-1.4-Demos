# Assignment 89 – Direct Host Memory Image Blits & ReBAR Zero-Copy Texture Streaming

## Overview & Architectural Critique
On modern platforms with Resizable BAR (ReBAR) or unified memory architectures, allocating intermediate staging buffers is redundant. **Assignment 89** leverages `VK_EXT_host_image_copy` combined with host-visible, device-local memory to stream dynamic procedural textures directly from CPU memory into optimal GPU tiled images with zero intermediate copies and zero command buffer recording overhead.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_host_image_copy`**: Host-side image copies (`vkCopyMemoryToImageEXT`).
- **Zero Staging Buffer Allocations**: Direct host memory transfer across the PCIe bus.
- **Dynamic Rendering**: Rendering sampled zero-copy textures onto 3D surfaces.

## Acceptance Criteria
- [x] Configure host image copy structures and capabilities.
- [x] Stream procedural CPU textures directly into GPU image memory every frame.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Zero-copy host image copy pipeline.
- `shaders/rebar_blit.vert`, `shaders/rebar_blit.frag`: Blit shaders.
- `CMakeLists.txt`: Build target configuration.
