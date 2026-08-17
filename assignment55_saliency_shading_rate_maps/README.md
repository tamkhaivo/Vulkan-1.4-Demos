# Assignment 55 – Saliency Shading Rate Maps & Dynamic Foveated VRS (`VK_KHR_fragment_shading_rate` Attachment)

## Overview
Compute real-time shading rate maps on the GPU based on visual saliency, camera motion vectors, and gaze point tracking, feeding the dynamic rate map back as a fragment shading rate attachment in a subsequent dynamic rendering pass.

## Key Concepts
- Dynamic Shading Rate Image generation via Compute Shader ($R8\_UINT$).
- Attachment shading rate configuration (`VkRenderingFragmentShadingRateAttachmentInfoKHR`).
- Combiner operations (`VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR`, `VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR`).
- Variable rate foveation ($1\times1$ at gaze center, decaying to $2\times2$ and $4\times4$ in peripheral zones).

## Acceptance Criteria
- [x] Query physical device fragment shading rate attachment properties.
- [x] Generate dynamic $R8\_UINT$ shading rate texture using compute kernel.
- [x] Attach shading rate map via `VkRenderingFragmentShadingRateAttachmentInfoKHR`.
- [x] Render complex scene and verify 40%+ fragment invocation reduction in peripheral zones.
- [x] Validate proper synchronization between compute write and raster attachment read.

## Directory Structure
- `src/main.cpp`: Dynamic shading rate map generator and VRS attachment renderer.
- `CMakeLists.txt`: Build target configuration.
