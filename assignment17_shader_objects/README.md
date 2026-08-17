# Assignment 17 – Next-Gen Pipeline Flexibility with Shader Objects (`VK_EXT_shader_object`)

## Overview & Architectural Critique
Monolithic Pipeline State Objects (`VkPipeline`) require bundling vertex, fragment, rasterization, blending, and viewport states into immutable objects compiled upfront. This causes pipeline explosion, lengthy PSO pre-warming times, and excessive runtime memory usage.

In Vulkan 1.4, **Shader Objects (`VK_EXT_shader_object`)** decouples shader stage compilation completely. Shaders are compiled independently into `VkShaderEXT` objects and dynamically bound in command buffers using `vkCmdBindShadersEXT`. All pipeline fixed-function state is dynamically controlled via Extended Dynamic State commands (`vkCmdSetCullMode`, `vkCmdSetDepthTestEnable`, `vkCmdSetColorBlendEquationEXT`).

## Key Vulkan 1.4 Concepts
- **`VK_EXT_shader_object` Features**: `shaderObject = VK_TRUE`.
- **Decoupled Creation**: `vkCreateShadersEXT` compiling standalone SPIR-V binaries into `VkShaderEXT`.
- **Dynamic Shader Binding**: `vkCmdBindShadersEXT(cmd, stageCount, pStages, pShaders)`.
- **Dynamic State Completeness**: Full dynamic state specification before draw calls (`vkCmdSetViewportWithCount`, `vkCmdSetScissorWithCount`, `vkCmdSetVertexInputEXT`, `vkCmdSetRasterizerDiscardEnable`).

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Standalone Vertex Shader Object
VkShaderCreateInfoEXT vertShaderInfo{
    .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
    .codeSize = vertSpirvCode.size() * sizeof(uint32_t),
    .pCode = vertSpirvCode.data(),
    .pName = "main",
    .setLayoutCount = 1,
    .pSetLayouts = &descriptorSetLayout,
    .pushConstantRangeCount = 0
};
VkShaderEXT vertShader;
vkCreateShadersEXT(device, 1, &vertShaderInfo, nullptr, &vertShader);

// 2. Command Buffer Recording: Binding Shader Objects & Dynamic States
vkCmdBeginRendering(cmd, &renderInfo);

VkShaderStageFlagBits stages[] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
VkShaderEXT shaders[] = { vertShader, fragShader };
vkCmdBindShadersEXT(cmd, 2, stages, shaders);

// Set all dynamic state without monolithic VkPipeline
vkCmdSetViewportWithCount(cmd, 1, &viewport);
vkCmdSetScissorWithCount(cmd, 1, &scissor);
vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);
vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
vkCmdSetDepthTestEnable(cmd, VK_TRUE);
vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS_OR_EQUAL);

// Set Dynamic Vertex Input State
vkCmdSetVertexInputEXT(cmd, 1, &bindingDesc, 2, attributeDescs);

vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_shader_object` features.
- [x] Create standalone vertex and fragment `VkShaderEXT` objects using `vkCreateShadersEXT`.
- [x] Bind shaders dynamically in command buffer using `vkCmdBindShadersEXT` without creating `VkPipeline`.
- [x] Configure complete dynamic rasterization, blend, depth, and vertex input states via `vkCmdSet*` API commands.
- [x] Swap fragment shaders at runtime with zero PSO compilation hitch or pipeline recreation.

## Directory Structure
- `src/main.cpp`: Shader objects implementation source code.
- `shaders/shader_obj.vert`, `shaders/shader_obj.frag`: Standalone SPIR-V shaders.
- `CMakeLists.txt`: Build target configuration.
