# Assignment 56 – Dynamic Multi-Draw Shader Indirect with Graphics Pipeline Tokens (`VK_EXT_device_generated_commands`)

## Overview
Execute completely autonomous GPU-driven rendering pipelines where the GPU generates draw calls, vertex/index bindings, push constants, and switches graphic pipeline states dynamically via Device Generated Commands token streams.

## Key Concepts
- `VK_EXT_device_generated_commands` (DGC) with `VkIndirectCommandsLayoutEXT`.
- Token streams (`PIPELINE`, `PUSH_CONSTANT`, `DRAW_INDEXED`).
- Preprocessing token buffers on GPU with `vkCmdPreprocessGeneratedCommandsEXT`.
- Executing multi-pipeline draws with `vkCmdExecuteGeneratedCommandsEXT`.

## Acceptance Criteria
- [x] Query and enable `deviceGeneratedCommands` features.
- [x] Create `VkIndirectCommandsLayoutEXT` declaring pipeline, push constant, and draw tokens.
- [x] Preprocess generated token buffers on the GPU.
- [x] Execute heterogeneous draw batches via `vkCmdExecuteGeneratedCommandsEXT`.
- [x] Verify zero CPU draw call loop overhead.

## Directory Structure
- `src/main.cpp`: Device generated commands setup and indirect token dispatcher.
- `CMakeLists.txt`: Build target configuration.
