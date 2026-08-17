# Assignment 72 – Ultra-Low-Latency Reflex Pacing & Input-to-Photon Instrumentation (VK_NV_low_latency2)

## Overview & Architectural Critique
In competitive and VR rendering workloads, CPU frame generation frequently outruns the GPU, filling the swapchain submit queue with 2 to 4 frames of buffered commands. This causes click-to-photon latency spikes of 40ms to 80ms.

**Assignment 72** implements the **Vulkan Low Latency 2 API (`VK_NV_low_latency2`)**:
1. **Adaptive Frame Sleep (`vkLatencySleepNV`)**: Sleeps the CPU main thread until the exact nanosecond required to sample player input before GPU rendering commences.
2. **Latency Marker Telemetry (`vkSetLatencyMarkerNV`)**: Accurately instruments every frame stage (`SIMULATION_START`, `SIMULATION_END`, `INPUT_SAMPLE`, `RENDERSUBMIT_START`, `RENDERSUBMIT_END`, `PRESENT_START`, `PRESENT_END`).
3. **Present ID Chain Association (`VkLatencySubmissionPresentIdNV`)**: Associates individual draw batches and monotonic timeline values directly with presentation hardware timestamps.

## Key Vulkan 1.4 Concepts
- **`VK_NV_low_latency2`**: Hardware-assisted pacing reducing frame queue depth from 3+ frames to $< 0.5$ frames.
- **Latency Instrumentation**: `vkGetLatencyTimingsNV` pulling millisecond/microsecond breakdown of input-to-photon delays.
- **Dynamic Rendering & Synchronization2**: Minimal overhead frame submission pipeline.

## Acceptance Criteria
- [x] Query and configure `VK_NV_low_latency2` extension and physical device features.
- [x] Inject complete suite of latency markers into simulation and render loop.
- [x] Execute adaptive `vkLatencySleepNV` CPU throttling.
- [x] Render moving scene with low latency instrumentation and 0 validation errors.

## Directory Structure
- `src/main.cpp`: Low-latency host engine and telemetry logging loop.
- `shaders/latency_cube.vert`, `shaders/latency_cube.frag`: 3D test mesh shaders.
- `CMakeLists.txt`: Build target configuration.
