# Assignment 45 – Robustness2, Pipeline Robustness & Fault Tolerance (`VK_EXT_robustness2` / `VK_EXT_pipeline_robustness`)

## Overview
Harden Vulkan 1.4 rendering architectures against device lost errors, out-of-bounds shader crashes, and uninitialized descriptor reads using fine-grained per-pipeline robustness (`VK_EXT_pipeline_robustness`) and null descriptor safety.

## Key Concepts
- `VK_EXT_robustness2` (`robustBufferAccess2`, `robustImageAccess2`, `nullDescriptor`).
- `VK_EXT_pipeline_robustness` for configuring robustness per-pipeline and per-stage without global driver overhead.
- Safe default returns for out-of-bounds buffer loads/stores (`0` reads, discarded writes).
- Binding `VK_NULL_HANDLE` descriptor resources without triggering GPU driver page faults or TDRs.
- Graceful recovery and breadcrumb debugging with `VK_EXT_device_fault`.

## Acceptance Criteria
- [x] Query and enable `robustness2` and `pipelineRobustness` physical device features.
- [x] Configure `VkPipelineRobustnessCreateInfoEXT` with granular settings (`VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT`).
- [x] Intentionally trigger out-of-bounds buffer accesses in vertex and compute shaders and verify deterministic zero-read / write-discard behavior.
- [x] Test null descriptor bindings (`nullDescriptor` feature) on unbounded bindless descriptor arrays.
- [x] Ensure application runs without driver TDR crashes or validation layer errors under stress testing.

## Directory Structure
- `src/main.cpp`: Pipeline robustness configuration, null descriptor test, and safety validation host application.
- `CMakeLists.txt`: Build target configuration.
