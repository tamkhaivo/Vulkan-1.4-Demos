# Assignment 49 – Low Latency Swapchain Timing & Latency Sleep (`VK_NV_low_latency2` / `VK_EXT_present_timing`)

## Overview
Integrate hardware low-latency pacing (Vulkan Low Latency 2 / Reflex) to eliminate GPU render queue bloat, minimize click-to-photon latency, and synchronize CPU simulation loops directly with presentation VBLANK.

## Key Concepts
- Low-latency mode configuration (`VK_NV_low_latency2`, `VkSetLatencyMarkerInfoNV`).
- Latency sleep markers (`vkLatencySleepNV`) inserted directly prior to input polling.
- Tracking latency markers: `SIMULATION_START`, `INPUT_SAMPLE`, `RENDERSUBMIT_START`, `PRESENT_START`.
- Measuring sub-millisecond end-to-end frame latency.

## Acceptance Criteria
- [x] Query physical device support for `VK_NV_low_latency2` / present timing.
- [x] Configure low latency mode and boost flags.
- [x] Insert `vkLatencySleepNV` before game simulation and input sampling steps.
- [x] Tag submission milestones with `vkSetLatencyMarkerNV`.
- [x] Verify reduced frame latency compared to traditional unbounded queues.

## Directory Structure
- `src/main.cpp`: Low-latency timing configuration and telemetry host application.
- `CMakeLists.txt`: Build target configuration.
