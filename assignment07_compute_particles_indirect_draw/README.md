# Assignment 7 – Compute Particle System with Indirect Draw

## Overview
Simulate thousands of particles in a compute shader, store state in SSBOs, and render via indirect draw commands (`vkCmdDrawIndirect`) without CPU readback.

## Key Concepts
- Compute pipeline creation & `vkCmdDispatch`.
- Shader Storage Buffer Objects (SSBO).
- GPU indirect drawing parameters (`VkDrawIndirectCommand`).
- Pipeline barriers synchronizing Compute Write -> Graphics Vertex/Indirect Read.
