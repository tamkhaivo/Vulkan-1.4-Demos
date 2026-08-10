# Assignment 4 – Push Constants and Dynamic Uniform Buffers

## Overview
Draw multiple 3D objects with different transformations using push constants and a shared material uniform buffer with dynamic offsets.

## Key Concepts
- Push constants (`VkPushConstantRange`, `vkCmdPushConstants`) for high-frequency small updates (model matrices).
- Dynamic uniform buffers (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`) for per-object material bindings using dynamic offsets.
