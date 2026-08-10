# Assignment 2 – Rotating Cube with Uniform Buffers

## Overview
Render a 3D rotating cube with a model-view-projection (MVP) matrix updated each frame via a uniform buffer.

## Key Concepts
- Device-local vertex buffer & host-visible staging buffers.
- `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` for MVP matrix storage.
- Descriptor set layout, pool, allocation, and updating via `vkUpdateDescriptorSets`.
- Pipeline layout linking descriptors to graphics pipeline.
