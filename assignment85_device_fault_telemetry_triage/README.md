# Assignment 85 – Direct GPU Memory Fault Triage & Page-Fault Telemetry

## Overview & Architectural Critique
When asynchronous GPU compute kernels dereference out-of-bounds 64-bit Buffer Device Addresses (BDA), hardware engines trigger catastrophic TDRs or `VK_ERROR_DEVICE_LOST` with zero CPU callstack fidelity. **Assignment 85** integrates `VK_EXT_device_fault` and `VK_EXT_device_address_binding_report` principles into a crash telemetry reporting harness that maps 64-bit GPU virtual addresses to exact buffer boundaries and triage telemetry.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_device_fault` & BDA Triage**: Virtual address interval trees.
- **64-bit Pointer Tracking**: Dereferencing and tracking bounds on device.
- **Dynamic Rendering**: Rendering interactive telemetry diagnostic overlay.

## Acceptance Criteria
- [x] Register GPU memory intervals for active 64-bit BDA buffers.
- [x] Render real-time BDA address diagnostic visualization.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Fault telemetry and BDA monitoring pipeline.
- `shaders/fault_triage.vert`, `shaders/fault_triage.frag`: Telemetry shaders.
- `CMakeLists.txt`: Build target configuration.
