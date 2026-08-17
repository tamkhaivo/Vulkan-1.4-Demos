# Assignment 19 – Hardware Variable Rate Shading & Density Maps (`VK_KHR_fragment_shading_rate`)

## Overview
Optimize fragment shader fill-rate and rendering throughput by dynamically controlling fragment shading rates (e.g. 1x1, 2x2, 4x4) via dynamic pipeline states and fragment density maps.

## Key Concepts
- `VK_KHR_fragment_shading_rate` feature enablement (`pipelineFragmentShadingRate`, `attachmentFragmentShadingRate`).
- Setting dynamic shading rates per draw via `vkCmdSetFragmentShadingRateKHR`.
- Shading rate image attachments (VRS density maps / foveated rate maps).
- Shading rate combiners (`VkFragmentShadingRateCombinerOpKHR`) for blending primitive, pipeline, and attachment rates.

## Acceptance Criteria
- [x] Enable `pipelineFragmentShadingRate` and shading rate extensions during device creation.
- [x] Load `vkCmdSetFragmentShadingRateKHR` function pointer.
- [x] Configure graphics pipeline with dynamic fragment shading rate state enabled.
- [x] Dynamically adjust fragment shading rates (1x1, 2x2, 4x4) per draw call using `vkCmdSetFragmentShadingRateKHR`.
- [x] Render 3D geometry observing fragment rate variations across the viewport.

## Directory Structure
- `src/main.cpp`: Variable rate shading host application.
- `shaders/`: GLSL shaders for VRS rendering.
- `CMakeLists.txt`: Build target configuration.
