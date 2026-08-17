# Assignment 2 – Rotating Cube with Uniform Buffers

## Overview
Render a 3D rotating cube with perspective projection, depth buffering, and per-vertex colors, updating a Model-View-Projection (MVP) transformation matrix every frame via Uniform Buffer Objects (UBOs) and descriptor sets.

## Key Concepts
- Device-local vertex and index buffers with host-visible staging buffer memory copies.
- Uniform Buffer Object (UBO) allocation and continuous host mapping (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`).
- Descriptor Set Layout (`VkDescriptorSetLayout`), Descriptor Pool (`VkDescriptorPool`), and Descriptor Set allocation.
- Updating descriptor sets via `vkUpdateDescriptorSets` to bind uniform buffer memory.
- Dynamic depth testing with `VK_FORMAT_D32_SFLOAT` attachment in dynamic rendering.
- Real-time MVP matrix computation using Vulkan clip coordinates (Y-inverted, Z in [0, 1]).

## Acceptance Criteria
- [x] Create vertex and index buffers for a 3D colored cube and transfer geometry data via staging buffers.
- [x] Create and persistently map uniform buffers holding `model`, `view`, and `proj` 4x4 transformation matrices.
- [x] Configure `VkDescriptorSetLayout`, `VkDescriptorPool`, and write descriptor sets with `VkDescriptorBufferInfo`.
- [x] Create a graphics pipeline with depth testing enabled (`VkPipelineDepthStencilStateCreateInfo`) and dynamic rendering depth attachment format `VK_FORMAT_D32_SFLOAT`.
- [x] Update cube rotation angle dynamically each frame in the render loop and write to the mapped UBO memory.
- [x] Bind pipeline and descriptor sets (`vkCmdBindDescriptorSets`), draw indexed cube mesh (`vkCmdDrawIndexed`) within `vkCmdBeginRendering`.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`cube.vert`, `cube.frag`).
- `CMakeLists.txt`: Build target configuration.
