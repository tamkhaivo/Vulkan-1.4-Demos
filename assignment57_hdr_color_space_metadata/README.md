# Assignment 57 – High Dynamic Range (HDR10) Color Space Management (`VK_EXT_hdr_metadata`)

## Overview & Architectural Critique
Standard dynamic range (SDR) rendering clamps output luminance to 80–100 nits under sRGB / Rec.709 color primaries. High Dynamic Range (HDR) displays support up to 1,000–10,000 nits and wide BT.2020 color gamuts.

In Vulkan 1.4, **HDR Color Space Management (`VK_EXT_hdr_metadata`)** allows configuring swapchains targeting `VK_COLOR_SPACE_HDR10_ST2084_EXT` (PQ transfer function) or `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` (scRGB). By calling `vkSetHdrMetadataEXT`, the engine supplies mastering display primaries, min/max luminance, MaxCLL (Content Light Level), and MaxFALL to the physical display for optimal tone-mapping.

## Key Vulkan 1.4 Concepts
- **HDR Swapchain Formats**: `VK_FORMAT_A2B10G10R10_UNORM_PACK32` with `VK_COLOR_SPACE_HDR10_ST2084_EXT` or `VK_FORMAT_R16G16B16A16_SFLOAT` with scRGB.
- **Display Metadata**: `VkHdrMetadataEXT` specifying CIE 1931 xy color coordinates and luminance ranges in nits.
- **Tonemapping Pipeline**: Fragment shader encoding linear scene luminance into SMPTE ST 2084 Perceptual Quantizer (PQ) curves.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure HDR10 Display Metadata
VkHdrMetadataEXT hdrMetadata{
    .sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
    .pNext = nullptr,
    // BT.2020 Primaries
    .displayPrimaryRed = { 0.708f, 0.292f },
    .displayPrimaryGreen = { 0.170f, 0.797f },
    .displayPrimaryBlue = { 0.131f, 0.046f },
    .whitePoint = { 0.3127f, 0.3290f }, // D65 White Point
    .maxLuminance = 1000.0f,            // 1,000 nits peak
    .minLuminance = 0.001f,             // 0.001 nits black level
    .maxContentLightLevel = 1000.0f,     // MaxCLL
    .maxFrameAverageLightLevel = 400.0f  // MaxFALL
};

// Set HDR metadata on the active swapchain
vkSetHdrMetadataEXT(device, 1, &swapchain, &hdrMetadata);
```

## Acceptance Criteria
- [x] Query HDR surface color space support (`VK_COLOR_SPACE_HDR10_ST2084_EXT` or scRGB).
- [x] Create 10-bit or 16-bit float swapchain matching HDR surface capabilities.
- [x] Set display metadata parameters via `vkSetHdrMetadataEXT`.
- [x] Implement ST 2084 PQ encoding curve tonemapper in fragment shader.
- [x] Verify wide color gamut rendering with 100% clean validation output.

## Directory Structure
- `src/main.cpp`: HDR color space host application.
- `shaders/hdr_tonemap.vert`, `shaders/hdr_tonemap.frag`: ST 2084 PQ shaders.
- `CMakeLists.txt`: Build target configuration.
