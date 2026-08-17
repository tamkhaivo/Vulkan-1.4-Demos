# Assignment 27 – Extended Dynamic State 3 & Vulkan 1.4 Dynamic Pipelines (`VK_EXT_extended_dynamic_state3`)

## Overview
Eliminate pipeline state object (PSO) combinatorial explosion by controlling rasterization, blending, polygon modes, and sample locations dynamically at command recording time.

## Key Concepts
- `VK_EXT_extended_dynamic_state3` (and Extended Dynamic State 1 & 2).
- Dynamic polygon modes: `vkCmdSetPolygonModeEXT` (wireframe vs solid fill).
- Dynamic color blend enable, blend equations, and write masks (`vkCmdSetColorBlendEnableEXT`, `vkCmdSetColorBlendEquationEXT`).
- Dynamic depth clamp, rasterizer discard, and sample locations without creating new monolithic pipelines.

## Acceptance Criteria
- [x] Enable `extendedDynamicState3PolygonMode`, `extendedDynamicState3ColorBlendEquation`, and related features.
- [x] Create a single baseline graphics pipeline with dynamic states declared in `VkPipelineDynamicStateCreateInfo`.
- [x] Dynamically switch between wireframe and solid fill rendering on consecutive draw calls using dynamic state commands.
- [x] Dynamically modify blend equations and color write masks without pipeline rebinding.
- [x] Verify error-free rendering with validation layers confirming proper dynamic state setting.

## Directory Structure
- `src/main.cpp`: Extended dynamic state host application.
- `shaders/`: GLSL shaders for dynamic state rendering.
- `CMakeLists.txt`: Build target configuration.
