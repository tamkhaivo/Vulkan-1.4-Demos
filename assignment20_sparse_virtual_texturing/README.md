# Assignment 20 – Sparse Virtual Texturing & Residency Streaming (`sparseResidencyImage2D`)

## Overview
Implement sparse virtual textures (SVT / megatexturing) using Vulkan sparse resources, binding physical memory pages on demand and handling residency mip-tails.

## Key Concepts
- Sparse image creation with `VK_IMAGE_CREATE_SPARSE_BINDING_BIT` and `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT`.
- Sparse memory requirements querying via `vkGetImageSparseMemoryRequirements`.
- On-demand sparse memory page binding using `vkQueueBindSparse`.
- Mip-tail single-page packing and sparse residency shader sampling (`sparseTextureARB` / SPIR-V residency codes).

## Acceptance Criteria
- [x] Verify sparse image residency support on physical device features (`sparseBinding`, `sparseResidencyImage2D`).
- [x] Create a sparse 2D image and query tile granularity and mip-tail properties.
- [x] Allocate physical `VkDeviceMemory` pages for requested virtual texture tiles.
- [x] Bind memory pages dynamically using `vkQueueBindSparse` with timeline/binary semaphores.
- [x] Sample sparse textures in fragment shaders safely handling uncommitted memory pages.

## Directory Structure
- `src/main.cpp`: Sparse virtual texturing host application.
- `shaders/`: GLSL shaders for sparse texture sampling.
- `CMakeLists.txt`: Build target configuration.
