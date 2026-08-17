# Assignment 7 – Compute Particle System with Indirect Draw

## Overview
Simulate hundreds of thousands of dynamic particles entirely on the GPU using a Vulkan compute shader, update positions and lifetimes in a Shader Storage Buffer Object (SSBO), and render them using GPU indirect drawing (`vkCmdDrawIndirect`) with zero CPU readback.

## Key Concepts
- Compute pipeline creation (`VkComputePipelineCreateInfo`) and compute shader dispatch (`vkCmdDispatch`).
- Shader Storage Buffer Objects (`VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`) for high-throughput GPU read/write physics states.
- GPU Indirect Drawing with `VkDrawIndirectCommand` (`vertexCount`, `instanceCount`, `firstVertex`, `firstInstance`).
- Vulkan 1.4 Synchronization2 pipeline barriers synchronizing Compute Shader Write -> Vertex Input / Indirect Draw Command Read.
- Dynamic rendering with additive blending for luminous particle effects.

## Acceptance Criteria
- [x] Initialize compute and graphics pipeline layouts and descriptor set layouts with SSBO bindings.
- [x] Populate an initial SSBO containing thousands of particle states (position, velocity, life, color).
- [x] Create an indirect draw command buffer with `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`.
- [x] Dispatch compute shader to update particle physics and populate/update the indirect draw arguments buffer.
- [x] Transition buffer memory hazards using `VkBufferMemoryBarrier2` (`COMPUTE_SHADER_WRITE_BIT` -> `INDIRECT_COMMAND_READ_BIT | VERTEX_ATTRIBUTE_READ_BIT`).
- [x] Issue `vkCmdDrawIndirect` in dynamic rendering to render the active particles without CPU sync.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: Compute and graphics shaders (`particle.comp`, `particle.vert`, `particle.frag`).
- `CMakeLists.txt`: Build target configuration.
