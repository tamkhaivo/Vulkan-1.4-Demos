# Assignment 42 – Custom Border Colors & Advanced Sampler Swizzling (`VK_EXT_custom_border_color` / `VK_EXT_border_color_swizzle`)

## Overview
Implement custom texture clamping boundaries and runtime component swizzling for texture atlases, shadow map clamping, and terrain splatting using arbitrary RGBA custom border values without sampler reallocation.

## Key Concepts
- `VK_EXT_custom_border_color` and `VK_EXT_border_color_swizzle` extensions.
- `VkSamplerCustomBorderColorCreateInfoEXT` with arbitrary `VkClearColorValue`.
- Clamping modes: `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER` with custom float/integer borders.
- Texture channel component mapping (`VkComponentMapping`) runtime swizzle in sampler lookups.
- Shadow map border bias and depth clamp handling to prevent shadow leakage on screen edges.

## Acceptance Criteria
- [x] Enable custom border color physical device features.
- [x] Create samplers using `VkSamplerCustomBorderColorCreateInfoEXT` specifying custom non-standard border values (e.g., specific transparent keys or out-of-bounds depth values).
- [x] Apply the sampler to shadow mapping and texture masking to prevent bleeding artifacts at texture boundaries.
- [x] Render dynamic textured geometry demonstrating clean border clamping behavior.

## Directory Structure
- `src/main.cpp`: Custom border color sampler setup and texture rendering host application.
- `CMakeLists.txt`: Build target configuration.
