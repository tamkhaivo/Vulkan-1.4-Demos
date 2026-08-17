# Assignment 49 – Low Latency Swapchain Timing & Latency Sleep (`VK_NV_low_latency2`)

## Overview & Architectural Critique
In competitive gaming and real-time interactive simulations, CPU threads often render frames ahead of the GPU, filling the frame queue and increasing input-to-display (click-to-photon) latency by several frames.

In Vulkan 1.4, **Low Latency Frame Pacing (`VK_NV_low_latency2` / NVIDIA Reflex API in Vulkan)** allows the application to synchronize CPU input gathering directly with GPU start-of-frame execution. By inserting `vkLatencySleepNV` and querying `vkGetLatencyTimingsNV`, the CPU sleeps until the GPU is ready for the next frame, reducing input latency by up to 50% without lowering framerates.

## Key Vulkan 1.4 Concepts
- **`VK_NV_low_latency2` Feature**: `lowLatency2 = VK_TRUE`.
- **Latency Sleep**: `vkLatencySleepNV` throttling CPU frame kickoff to align with GPU readiness.
- **Latency Markers**: `vkSetLatencyMarkerNV` recording simulation start/end, render submit start/end, and present markers.
- **Timing Telemetry**: `vkGetLatencyTimingsNV` querying per-frame input latency in nanoseconds.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Low Latency Mode on Swapchain
VkLatencySubmissionPresentIdNV presentIdInfo{
    .sType = VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV,
    .presentID = currentFrameId
};

VkLatencySleepInfoNV sleepInfo{
    .sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV,
    .signalSemaphore = latencySleepSemaphore,
    .value = currentFrameId
};

// 2. Main Frame Loop with Low Latency Sleep
void renderFrame() {
    // Sleep CPU until optimal frame start time to minimize input latency
    vkLatencySleepNV(device, swapchain, &sleepInfo);

    // Record Simulation Start Marker
    VkSetLatencyMarkerInfoNV markerStart{
        .sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV,
        .presentID = currentFrameId,
        .marker = VK_LATENCY_MARKER_SIMULATION_START_NV
    };
    vkSetLatencyMarkerNV(device, swapchain, &markerStart);

    // Read User Input & Update Simulation State
    processInput();

    // Record Render Submit Start Marker
    VkSetLatencyMarkerInfoNV markerRender{
        .sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV,
        .presentID = currentFrameId,
        .marker = VK_LATENCY_MARKER_RENDERSUBMIT_START_NV
    };
    vkSetLatencyMarkerNV(device, swapchain, &markerRender);

    // Record & Submit Command Buffers...
}
```

## Acceptance Criteria
- [x] Query and enable `VK_NV_low_latency2` physical device features.
- [x] Integrate `vkLatencySleepNV` at the start of the frame loop to eliminate CPU frame queue buildup.
- [x] Record standard latency markers (`SIMULATION_START`, `RENDERSUBMIT_START`, `PRESENT_START`).
- [x] Query and display real-time click-to-photon latency metrics.
- [x] Verify reduced frame latency with smooth 60+ FPS presentation and clean validation output.

## Directory Structure
- `src/main.cpp`: Low latency swapchain timing host application.
- `shaders/low_latency.vert`, `shaders/low_latency.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
