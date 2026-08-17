# Assignment 26 – Device Generated Commands (DGC) in Vulkan 1.4

## Overview
Generate and execute dynamic graphics draw commands and state bindings directly on the GPU using Device Generated Commands (`VK_NV_device_generated_commands` / `VK_EXT_device_generated_commands`).

## Key Concepts
- `VkIndirectCommandsLayoutNV` / `VkIndirectExecutionSetEXT` token streams.
- Supported token types: draw calls, index buffer binds, vertex buffer binds, and dynamic push constants.
- GPU command preprocessing via `vkCmdPreprocessGeneratedCommandsNV`.
- Direct execution via `vkCmdExecuteGeneratedCommandsNV` with zero CPU draw loop intervention.

## Acceptance Criteria
- [x] Query and enable Device Generated Commands features and device extensions.
- [x] Create indirect command layouts defining token sequences (push constants + draw commands).
- [x] Populate input command token buffers on GPU or host-visible device memory.
- [x] Preprocess generated commands on GPU command streams.
- [x] Execute generated commands inside active dynamic render passes.

## Directory Structure
- `src/main.cpp`: Device generated commands host application.
- `shaders/`: GLSL shaders for DGC execution.
- `CMakeLists.txt`: Build target configuration.
