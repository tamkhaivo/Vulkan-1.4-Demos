# Assignment 26 – Device Generated Commands (`VK_NV_device_generated_commands` / `VK_EXT_device_generated_commands`)

## Overview & Architectural Critique
Standard GPU-driven rendering (`vkCmdDrawIndirect`) allows the GPU to control draw arguments (vertex counts, instance counts), but the CPU must still pre-record all pipeline bindings, push constants, and descriptor updates.

In Vulkan 1.4, **Device Generated Commands (DGC, `VK_EXT_device_generated_commands` / `VK_NV_device_generated_commands`)** empowers the GPU to generate an arbitrary stream of command tokens (Pipeline Binds, Push Constants, Vertex Buffers, Index Buffers, Draw / Dispatch calls) directly in device memory. The GPU preprocesses these tokens (`vkCmdPreprocessGeneratedCommandsNV` / `EXT`) and executes them autonomously via `vkCmdExecuteGeneratedCommandsNV`.

## Key Vulkan 1.4 Concepts
- **Indirect Commands Layout (`VkIndirectCommandsLayoutNV`)**: Declaring sequence of command tokens (e.g. `VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV`, `VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV`, `VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV`).
- **Indirect Execution Set (`VkIndirectExecutionSetEXT`)**: Collection of pipeline state objects that can be switched on-device.
- **Preprocessing & Execution**: `vkCmdPreprocessGeneratedCommandsNV` preparing device state, followed by `vkCmdExecuteGeneratedCommandsNV`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Define DGC Token Stream Layout
VkIndirectCommandsLayoutTokenNV tokens[3] = {
    // Token 0: Switch Pipeline State on GPU
    {
        .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV,
        .tokenType = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV,
        .stream = 0,
        .offset = 0
    },
    // Token 1: Write Push Constants on GPU
    {
        .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV,
        .tokenType = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV,
        .stream = 0,
        .offset = sizeof(uint32_t),
        .pushconstantPipelineLayout = pipelineLayout,
        .pushconstantShaderStageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pushconstantOffset = 0,
        .pushconstantSize = sizeof(glm::mat4)
    },
    // Token 2: Execute Indexed Draw Call on GPU
    {
        .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV,
        .tokenType = VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV,
        .stream = 0,
        .offset = sizeof(uint32_t) + sizeof(glm::mat4)
    }
};

VkIndirectCommandsLayoutCreateInfoNV layoutInfo{
    .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV,
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .tokenCount = 3,
    .pTokens = tokens,
    .streamStride = sizeof(DGCTokenPayload)
};
vkCreateIndirectCommandsLayoutNV(device, &layoutInfo, nullptr, &indirectCommandsLayout);

// 2. Preprocess and Execute Generated Commands on GPU
VkGeneratedCommandsInfoNV execInfo{
    .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV,
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .pipeline = defaultPipeline,
    .indirectCommandsLayout = indirectCommandsLayout,
    .streamCount = 1,
    .pStreams = &streamBinding,
    .sequencesCount = generatedSequenceCount,
    .preprocessBuffer = preprocessBuffer,
    .preprocessOffset = 0,
    .preprocessSize = preprocessBufferSize
};

vkCmdPreprocessGeneratedCommandsNV(cmd, &execInfo);
vkCmdBeginRendering(cmd, &renderInfo);
vkCmdExecuteGeneratedCommandsNV(cmd, VK_FALSE, &execInfo);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query and enable Device Generated Commands features (`deviceGeneratedCommands = VK_TRUE`).
- [x] Construct `VkIndirectCommandsLayoutNV` containing pipeline switch, push constant, and draw tokens.
- [x] Dispatch compute kernel to generate dynamic DGC token streams in GPU device memory.
- [x] Preprocess and execute command stream via `vkCmdExecuteGeneratedCommandsNV`.
- [x] Render multi-pipeline scene with zero CPU command recording during the frame loop.

## Directory Structure
- `src/main.cpp`: Device Generated Commands host application.
- `shaders/dgc_generator.comp`, `shaders/dgc_mesh.vert`, `shaders/dgc_mesh.frag`: DGC shaders.
- `CMakeLists.txt`: Build target configuration.
