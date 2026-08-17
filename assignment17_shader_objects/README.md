# Assignment 17 – Next-Gen Pipeline Flexibility with Shader Objects (`VK_EXT_shader_object`)

## Overview
Eliminate monolithic pipeline state object (`VkPipeline`) compilation and cache management by creating standalone, dynamically linked shader stages via `VK_EXT_shader_object`.

## Key Concepts
- `VK_EXT_shader_object` feature enablement (`VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject`).
- Standalone shader creation with `vkCreateShadersEXT` without building monolithic `VkPipeline` objects.
- Binding individual shader stages dynamically via `vkCmdBindShadersEXT`.
- Full dynamic state configuration: dynamic vertex input, dynamic blending, dynamic depth/stencil, dynamic rasterizer states.

## Acceptance Criteria
- [x] Enable `shaderObject` feature in logical device initialization.
- [x] Load `VK_EXT_shader_object` function pointers (`vkCreateShadersEXT`, `vkDestroyShaderEXT`, `vkCmdBindShadersEXT`, etc.).
- [x] Create independent vertex and fragment shader objects directly from SPIR-V bytecode.
- [x] Bind shader objects inside the command buffer using `vkCmdBindShadersEXT`.
- [x] Render geometry dynamically without creating or binding any `VkPipeline` objects.

## Directory Structure
- `src/main.cpp`: Shader objects Vulkan 1.4 host application.
- `shaders/`: Standalone GLSL shader sources.
- `CMakeLists.txt`: Build target configuration.
