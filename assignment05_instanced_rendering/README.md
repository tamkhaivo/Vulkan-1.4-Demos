# Assignment 5 – Instanced Rendering with Vertex Attribute Divisor

## Overview
Render thousands of cubes using instanced draw calls, using `VkPipelineVertexInputDivisorStateCreateInfoKHR` (core feature in Vulkan 1.4).

## Key Concepts
- Instancing with `vkCmdDrawIndexed`.
- Vertex input bindings with divisors (per-instance vs per-vertex attribute stepping).
- `gl_InstanceIndex` access in GLSL shaders.
