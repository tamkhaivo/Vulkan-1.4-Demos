# Assignment 86 – Sparse Dynamic Multi-Layer Virtual Megatexturing with Residency Feedback

## Overview & Architectural Critique
Open-world rendering engines require hundreds of gigabytes of unique landscape and surface textures that cannot fit into physical VRAM. **Assignment 86** builds a multi-layered Sparse Virtual Texture (SVT) megatexturing engine using `vkQueueBindSparse` and residency feedback buffers in fragment shaders, dynamically streaming $64\text{KB}$ physical memory pages on demand.

## Key Vulkan 1.4 Concepts
- **Sparse Virtual Texturing**: Virtual page table mapping and dynamic tile streaming.
- **Residency Feedback**: Fragment-driven missing page detection.
- **Dynamic Rendering**: Rendering sparse virtual megatextured terrain.

## Acceptance Criteria
- [x] Configure sparse virtual residency structures and mip-tail memory.
- [x] Render animated terrain mesh with multi-layered virtual texture streaming.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Sparse megatexturing runtime.
- `shaders/megatexture.vert`, `shaders/megatexture.frag`: SVT shaders.
- `CMakeLists.txt`: Build target configuration.
