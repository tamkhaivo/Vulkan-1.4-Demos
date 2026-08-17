# Assignment 54 – Device Diagnostic Checkpoints & GPU Fault Recovery (`VK_NV_device_diagnostic_checkpoints` / `VK_EXT_device_fault`)

## Overview
Implement production-grade GPU crash dump analysis, breadcrumb checkpoint markers, and register fault extraction to diagnose GPU Device Lost errors (`VK_ERROR_DEVICE_LOST`) and hardware TDRs.

## Key Concepts
- `VK_NV_device_diagnostic_checkpoints` (`vkCmdSetCheckpointNV`, `vkGetQueueCheckpointDataNV`).
- `VK_EXT_device_fault` for inspecting queryable address ranges, fault reasons, and memory access types.
- Inserting structured marker breadcrumbs around draw calls, compute dispatches, and memory barriers.
- Post-mortem checkpoint dumps on `VK_ERROR_DEVICE_LOST` to isolate exact offending GPU commands.

## Acceptance Criteria
- [x] Query and enable checkpoint and device fault extensions.
- [x] Insert execution checkpoint tags into command buffers with `vkCmdSetCheckpointNV`.
- [x] Intercept simulated hardware faults and dump queue checkpoints via `vkGetQueueCheckpointDataNV`.
- [x] Extract register fault address info with `vkGetDeviceFaultInfoEXT`.
- [x] Ensure deterministic post-mortem diagnosis of GPU device lost conditions.

## Directory Structure
- `src/main.cpp`: GPU checkpoint tracer, fault interceptor, and crash dump generator.
- `CMakeLists.txt`: Build target configuration.
