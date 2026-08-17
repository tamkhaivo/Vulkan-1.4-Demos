# Assignment 87 – Fine-Grained Programmable Raster Order Attachment Access & Lock-Free OIT

## Overview & Architectural Critique
Order-Independent Transparency (OIT) and per-pixel linked lists typically suffer from massive memory overheads and atomic contention. **Assignment 87** uses `VK_EXT_rasterization_order_attachment_access` principles to enable in-order read-modify-write access to color and depth attachments within a single subpass, producing deterministic multi-layer alpha blending without atomic locks or multi-pass sorting.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_rasterization_order_attachment_access`**: Programmable in-order blending.
- **Deterministic Multi-Layer Compositing**: Exact front-to-back blending.
- **Dynamic Rendering**: Rendering multi-layered intersecting transparent surfaces.

## Acceptance Criteria
- [x] Configure rasterization order attachment features and blend state.
- [x] Render intersecting multi-layer transparent glass sheets.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Raster order transparency pipeline.
- `shaders/raster_order_oit.vert`, `shaders/raster_order_oit.frag`: OIT shaders.
- `CMakeLists.txt`: Build target configuration.
