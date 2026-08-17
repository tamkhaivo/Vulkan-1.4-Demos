# Assignment 56 – Dynamic Multi-Draw Shader Indirect with Graphics Pipeline Tokens (`VK_EXT_device_generated_commands`)

## Overview & Architectural Critique
In large multi-material rendering pipelines, scenes contain thousands of objects utilizing different graphics pipelines (e.g. skinning vs static vs foliage vs water). Changing pipelines on the CPU stalls the draw loop.

In Vulkan 1.4, **Device Generated Commands with Multi-Pipeline Tokens (`VK_EXT_device_generated_commands`)** enables the GPU to dynamically switch graphics pipelines between consecutive indirect draws within a single token stream. The compute culling kernel writes pipeline indices into the token buffer, and `vkCmdExecuteGeneratedCommandsEXT` switches pipeline state directly on device silicon.

## Key Vulkan 1.4 Concepts
- **`VkIndirectExecutionSetEXT`**: Grouping multiple graphics pipelines into a single GPU-switchable set.
- **Pipeline Tokens**: `VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV` / `EXT` indexing into the execution set.
- **Unified GPU Draw Stream**: Interleaving pipeline switches, push constant updates, and draw commands in device memory.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Indirect Execution Set holding multiple Graphics Pipelines
VkIndirectExecutionSetPipelineInfoEXT setPipelineInfo{
    .sType = VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_PIPELINE_INFO_EXT,
    .initialPipeline = basePipeline,
    .maxPipelineCount = 4
};

VkIndirectExecutionSetCreateInfoEXT execSetInfo{
    .sType = VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT,
    .type = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT,
    .info = { .pPipelineInfo = &setPipelineInfo }
};
VkIndirectExecutionSetEXT executionSet;
vkCreateIndirectExecutionSetEXT(device, &execSetInfo, nullptr, &executionSet);

// Add other pipelines into the execution set
vkUpdateIndirectExecutionSetPipelineEXT(device, executionSet, 1, 1, &foliagePipeline);
vkUpdateIndirectExecutionSetPipelineEXT(device, executionSet, 2, 1, &waterPipeline);

// 2. Preprocess and Execute Dynamic Pipeline Token Stream
// (Executed on GPU with zero CPU pipeline rebinding)
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_device_generated_commands` multi-pipeline features.
- [x] Create `VkIndirectExecutionSetEXT` holding 3+ distinct material pipelines.
- [x] Generate DGC token buffer dynamically switching pipelines on the GPU.
- [x] Execute DGC draw stream and render multi-material scene with zero CPU draw loop intervention.

## Directory Structure
- `src/main.cpp`: DGC multi-pipeline token application.
- `shaders/dgc_multigen.comp`, `shaders/mat_static.frag`, `shaders/mat_water.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
