# Assignment 27 – Extended Dynamic State 3 & Vulkan 1.4 Dynamic Pipelines (`VK_EXT_extended_dynamic_state3`)

## Overview & Architectural Critique
In original Vulkan 1.0, state changes like Polygon Mode (Solid vs Wireframe), Rasterization Samples, Depth Clamp, Logic Ops, and Color Blend Equations required compiling separate `VkPipeline` objects for every permutation, causing PSO state explosion.

In Vulkan 1.4, **Extended Dynamic State 3 (`VK_EXT_extended_dynamic_state3` / `extendedDynamicState` 1 & 2 & 3)** makes nearly all fixed-function pipeline states dynamic. A single graphics pipeline can be created with dynamic state flags, allowing polygon modes, blend equations, primitive restart, and sample locations to be altered on-the-fly inside command buffers.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_extended_dynamic_state3` Features**: `extendedDynamicState3PolygonMode`, `extendedDynamicState3ColorBlendEquationEnable`, `extendedDynamicState3RasterizationSamples`.
- **Dynamic State Array**: Including `VK_DYNAMIC_STATE_POLYGON_MODE_EXT`, `VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT`, `VK_DYNAMIC_STATE_CULL_MODE`.
- **Direct Dynamic API Commands**: `vkCmdSetPolygonModeEXT`, `vkCmdSetColorBlendEquationEXT`, `vkCmdSetRasterizationSamplesEXT`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Graphics Pipeline with Extended Dynamic States
VkDynamicState dynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    VK_DYNAMIC_STATE_CULL_MODE,
    VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
    VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
};

VkPipelineDynamicStateCreateInfo dynamicStateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 5,
    .pDynamicStates = dynamicStates
};

// 2. Command Recording: Dynamic State Switching on a Single Pipeline
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, unifiedPipeline);

// Draw 1: Solid Rendering with Additive Blending
vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
VkColorBlendEquationEXT blendAdditive{
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD
};
vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendAdditive);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

// Draw 2: Wireframe Rendering with Alpha Blending (Same Pipeline!)
vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_LINE);
VkColorBlendEquationEXT blendAlpha{
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD
};
vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendAlpha);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Enable `VK_EXT_extended_dynamic_state3` features on physical device.
- [x] Create a single unified graphics pipeline configured with dynamic polygon mode and color blend equations.
- [x] Dynamically switch between solid fill and wireframe rendering via `vkCmdSetPolygonModeEXT` without PSO rebinding.
- [x] Dynamically switch color blend equations via `vkCmdSetColorBlendEquationEXT`.
- [x] Verify zero pipeline cache thrashing and clean validation layer output.

## Directory Structure
- `src/main.cpp`: Extended dynamic state 3 application source code.
- `shaders/dynamic_state.vert`, `shaders/dynamic_state.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
