# Assignment 28 – Calibrated Timestamps & Hardware Clock Profiling (`VK_KHR_calibrated_timestamps`)

## Overview & Architectural Critique
Profiling GPU execution time using standard `vkCmdWriteTimestamp` only measures GPU ticks. Converting these ticks into wall-clock CPU nanoseconds or correlating CPU physics jobs with GPU rendering passes requires calibrating clock domains simultaneously to prevent clock drift.

In Vulkan 1.4, **Calibrated Timestamps (`VK_KHR_calibrated_timestamps` / `VK_EXT_calibrated_timestamps`)** provides `vkGetCalibratedTimestampsEXT`, sampling the GPU device clock domain and the host CPU monotonic clock domain (`VK_TIME_DOMAIN_DEVICE_EXT` and `VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT` / `CLOCK_MONOTONIC_RAW_EXT`) in a single hardware-synchronized query.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_calibrated_timestamps` Feature**: Querying supported time domains with `vkGetPhysicalDeviceCalibrateableTimeDomainsEXT`.
- **Hardware Calibration Query**: `vkGetCalibratedTimestampsEXT` returning simultaneous CPU & GPU timestamps and calibration max deviation ($\sigma$).
- **Timestamp Query Pools**: `vkCmdWriteTimestamp2` with `VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT` and `VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT`.
- **Nanosecond Conversion**: Multiplying timestamp delta by `VkPhysicalDeviceLimits::timestampPeriod`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Simultaneously Sample Host CPU and GPU Device Timestamps
VkCalibratedTimestampInfoEXT timeInfo[2] = {
    {
        .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
        .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT
    },
    {
        .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
        .timeDomain = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT // Windows QPC
    }
};

uint64_t timestamps[2] = {0, 0};
uint64_t maxDeviation = 0;

vkGetCalibratedTimestampsEXT(device, 2, timeInfo, timestamps, &maxDeviation);

uint64_t gpuDeviceTicks = timestamps[0];
uint64_t cpuQpcTicks = timestamps[1];

// 2. Measure Precise Rendering Pass GPU Time via Timestamp Queries
vkCmdResetQueryPool(cmd, queryPool, 0, 2);

// Timestamp 0: Pass Start
vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, queryPool, 0);

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd);

// Timestamp 1: Pass End
vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, queryPool, 1);

// 3. Compute Elapsed Nanoseconds on Host
uint64_t queryResults[2] = {0, 0};
vkGetQueryPoolResults(device, queryPool, 0, 2, sizeof(queryResults), queryResults, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

float timestampPeriod = deviceProperties.limits.timestampPeriod; // nanoseconds per tick
double elapsedGpuNs = (queryResults[1] - queryResults[0]) * (double)timestampPeriod;
```

## Acceptance Criteria
- [x] Query calibrateable time domains via `vkGetPhysicalDeviceCalibrateableTimeDomainsEXT`.
- [x] Create a `VK_QUERY_TYPE_TIMESTAMP` query pool for per-pass GPU profiling.
- [x] Record start and end timestamps around render passes using `vkCmdWriteTimestamp2`.
- [x] Query simultaneous CPU-GPU calibrated timestamps using `vkGetCalibratedTimestampsEXT`.
- [x] Correlate CPU and GPU clock timelines and output nanosecond-precision profiling telemetry to console / overlay.

## Directory Structure
- `src/main.cpp`: Calibrated timestamps profiling host application.
- `shaders/profiled_scene.vert`, `shaders/profiled_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
