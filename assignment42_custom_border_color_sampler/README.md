# Assignment 42 – Custom Border Colors & Advanced Sampler Swizzling (`VK_EXT_custom_border_color`)

## Overview & Architectural Critique
Standard Vulkan samplers only support a fixed set of predefined border colors (`VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK`, `OPAQUE_BLACK`, `OPAQUE_WHITE`). For shadow map depth clamping, UI texture atlases, and custom LUT sampling, out-of-bounds UV coordinates often require arbitrary clear values (e.g. $(1.0, 1.0, 1.0, 1.0)$ for depth shadows or specific HDR coefficients) to prevent edge bleeding.

In Vulkan 1.4, **Custom Border Colors (`VK_EXT_custom_border_color` / Vulkan 1.4 Core)** allows specifying arbitrary `VkClearColorValue` arrays in `VkSamplerCustomBorderColorCreateInfoEXT` combined with `VK_BORDER_COLOR_FLOAT_CUSTOM_EXT`.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_custom_border_color` Feature**: `customBorderColors = VK_TRUE`, `customBorderColorWithoutFormat = VK_TRUE`.
- **`VkSamplerCustomBorderColorCreateInfoEXT`**: Defining arbitrary RGBA floating-point or integer border color values.
- **Sampler Clamp-To-Border**: Setting `addressModeU/V/W = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER` with `borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Arbitrary Custom Border Color (e.g. 1.0f for Shadow Map clamping)
VkSamplerCustomBorderColorCreateInfoEXT customBorderInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
    .pNext = nullptr,
    .customBorderColor = { .float32 = { 1.0f, 1.0f, 1.0f, 1.0f } }, // Out-of-bounds yields white
    .format = VK_FORMAT_D32_SFLOAT
};

VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext = &customBorderInfo,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT
};
vkCreateSampler(device, &samplerInfo, nullptr, &customSampler);
```

## Acceptance Criteria
- [x] Query and enable `customBorderColors` feature on physical device.
- [x] Create `VkSampler` configured with `VK_BORDER_COLOR_FLOAT_CUSTOM_EXT` and `VkSamplerCustomBorderColorCreateInfoEXT`.
- [x] Sample texture with coordinates outside $[0.0, 1.0]$ and verify exact custom border color clamping.
- [x] Confirm clean validation layer output without sampler configuration warnings.

## Directory Structure
- `src/main.cpp`: Custom border color sampler application source code.
- `shaders/custom_border.vert`, `shaders/custom_border.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
