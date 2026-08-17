# Assignment 4 – Push Constants and Dynamic Uniform Buffers

## Overview
Render multiple 3D objects across a scene with unique spatial transformations and material characteristics using push constants for low-latency per-draw updates and dynamic uniform buffers (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`) for per-object material bindings using dynamic byte offsets.

## Key Concepts
- Push constants (`VkPushConstantRange`, `vkCmdPushConstants`) for high-frequency lightweight updates (per-object model matrices, object IDs).
- Dynamic Uniform Buffers (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`) using `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment`.
- Binding descriptor sets with dynamic offsets via `vkCmdBindDescriptorSets` with `pDynamicOffsets`.
- Batch rendering multiple 3D entities in a single command buffer loop with varied transformations and shader material parameters.

## Acceptance Criteria
- [x] Configure `VkPipelineLayoutCreateInfo` with push constant ranges (`VK_SHADER_STAGE_VERTEX_BIT`) and dynamic uniform buffer bindings (`VK_SHADER_STAGE_FRAGMENT_BIT`).
- [x] Allocate and map a contiguous dynamic uniform buffer with elements aligned to `minUniformBufferOffsetAlignment`.
- [x] Write unique per-object material properties (albedo, roughness, metallic) to aligned memory slots.
- [x] In the render loop, iterate through objects and update push constants via `vkCmdPushConstants`.
- [x] Bind descriptor sets passing the corresponding dynamic byte offset into `vkCmdBindDescriptorSets`.
- [x] Draw multiple objects rendered with distinct colors and transformations in a single render pass.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`object.vert`, `object.frag`).
- `CMakeLists.txt`: Build target configuration.
