# Assignment 57 – High Dynamic Range (HDR10) Color Space Management & Swapchain Metadata (`VK_EXT_hdr_metadata`)

## Overview
Implement hardware HDR10 / scRGB wide color gamut rendering pipelines with mastering display metadata (`VkHdrMetadataEXT`), 16-bit floating point intermediate buffers, and dynamic Perceptual Quantizer (PQ / ST2084) tone mapping.

## Key Concepts
- HDR swapchain formats (`VK_FORMAT_A2B10G10R10_UNORM_PACK32`, `VK_FORMAT_R16G16B16A16_SFLOAT`).
- HDR Color spaces (`VK_COLOR_SPACE_HDR10_ST2084_EXT`, `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`).
- Setting HDR mastering metadata via `vkSetHdrMetadataEXT` (Max Luminance, Min Luminance, MaxCLL, MaxFALL).
- Linear-to-ST2084 (PQ) and scRGB color transform shaders.

## Acceptance Criteria
- [x] Query and select HDR-capable surface formats and color spaces.
- [x] Configure display mastering luminance metadata with `vkSetHdrMetadataEXT`.
- [x] Render scene into 16-bit floating point HDR render targets ($R16G16B16A16\_SFLOAT$).
- [x] Apply tonemapping / PQ curve encoding in final presentation pass.
- [x] Verify HDR pipeline execution and validation layer compliance.

## Directory Structure
- `src/main.cpp`: HDR surface configuration and metadata controller.
- `CMakeLists.txt`: Build target configuration.
