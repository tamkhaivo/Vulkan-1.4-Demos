# Assignment 28 – Calibrated Timestamps & GPU Hardware Profiling (`VK_KHR_calibrated_timestamps`)

## Overview
Perform high-precision GPU micro-benchmarking and engine frame pacing by correlating GPU clock domain query pools with CPU high-resolution monotonic clocks.

## Key Concepts
- `VK_KHR_calibrated_timestamps` (or `VK_EXT_calibrated_timestamps`) feature enablement.
- GPU Query Pools with `VK_QUERY_TYPE_TIMESTAMP`.
- Simultaneous CPU and GPU timestamp calibration via `vkGetCalibratedTimestampsEXT` / `vkGetCalibratedTimestampsKHR`.
- Converting raw GPU clock ticks into nanoseconds based on `VkPhysicalDeviceLimits::timestampPeriod`.

## Acceptance Criteria
- [x] Query supported time domains (`VK_TIME_DOMAIN_DEVICE_EXT`, `VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT`).
- [x] Create a timestamp query pool and record `vkCmdWriteTimestamp2` around render passes.
- [x] Calibrate CPU and GPU timestamps simultaneously via `vkGetCalibratedTimestampsEXT`.
- [x] Extract GPU timestamp delta, multiply by `timestampPeriod`, and calculate elapsed GPU execution time in microseconds/milliseconds.
- [x] Display real-time CPU/GPU frame times and timeline drift accurately.

## Directory Structure
- `src/main.cpp`: Calibrated timestamps and profiling host application.
- `shaders/`: GLSL shaders for benchmark rendering workloads.
- `CMakeLists.txt`: Build target configuration.
