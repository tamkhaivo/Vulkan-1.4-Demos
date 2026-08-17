# Assignment 54 – Device Diagnostic Checkpoints & Fault Recovery (`VK_NV_device_diagnostic_checkpoints` / `VK_EXT_device_fault`)

## Overview & Architectural Critique
When a GPU hang, illegal memory access, or OS Timeout Detection and Recovery (TDR) / Device Lost event occurs, diagnosing the exact command buffer and draw call that caused the crash is notoriously difficult because GPU execution is asynchronous.

In Vulkan 1.4, **Device Diagnostic Checkpoints (`VK_NV_device_diagnostic_checkpoints`)** and **Device Faults (`VK_EXT_device_fault`)** allow recording lightweight breadcrumb markers (`vkCmdSetCheckpointNV`) throughout command buffers. Following a `VK_ERROR_DEVICE_LOST`, the CPU queries `vkGetQueueCheckpointDataNV` and `vkGetDeviceFaultInfoEXT` to immediately pinpoint the exact crashing pass and dump GPU hardware fault registers.

## Key Vulkan 1.4 Concepts
- **`VK_NV_device_diagnostic_checkpoints` Feature**: `vkCmdSetCheckpointNV` and `vkGetQueueCheckpointDataNV`.
- **`VK_EXT_device_fault` Feature**: `deviceFault = VK_TRUE` and `vkGetDeviceFaultInfoEXT`.
- **Breadcrumb Markers**: Tagging command stream milestones with string/numeric identifiers.
- **Post-Mortem Crash Analysis**: Extracting queue checkpoint data upon `VK_ERROR_DEVICE_LOST`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Insert Diagnostic Checkpoint Markers into Command Stream
const char* pass1Marker = "G-Buffer Geometry Pass";
vkCmdSetCheckpointNV(cmd, (void*)pass1Marker);
vkCmdDrawIndexed(cmd, gbufferIndexCount, 1, 0, 0, 0);

const char* pass2Marker = "Ray Tracing Reflections Pass";
vkCmdSetCheckpointNV(cmd, (void*)pass2Marker);
vkCmdTraceRaysKHR(cmd, &rgenRegion, &missRegion, &hitRegion, &callRegion, width, height, 1);

// 2. Post-Mortem Inspection upon VK_ERROR_DEVICE_LOST
void handleDeviceLost(VkQueue queue) {
    uint32_t checkpointCount = 0;
    vkGetQueueCheckpointDataNV(queue, &checkpointCount, nullptr);
    std::vector<VkCheckpointDataNV> checkpoints(checkpointCount, { .sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
    vkGetQueueCheckpointDataNV(queue, &checkpointCount, checkpoints.data());

    for (const auto& cp : checkpoints) {
        const char* markerName = static_cast<const char*>(cp.pCheckpointMarker);
        std::cout << "[GPU Crash Log] Executed Checkpoint: " << markerName 
                  << " | Stage: " << cp.stage << std::endl;
    }
}
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_device_diagnostic_checkpoints` and `VK_EXT_device_fault` extensions.
- [x] Insert `vkCmdSetCheckpointNV` breadcrumb markers around every major rendering and compute pass.
- [x] Implement robust device lost handling querying `vkGetQueueCheckpointDataNV`.
- [x] Validate zero runtime performance overhead during normal execution.

## Directory Structure
- `src/main.cpp`: Diagnostic checkpoints host application.
- `shaders/diag_scene.vert`, `shaders/diag_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
