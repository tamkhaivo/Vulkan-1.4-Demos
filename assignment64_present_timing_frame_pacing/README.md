# Assignment 64 – Advanced Frame Pacing, Present Timing & Swapchain Feedback (`VK_EXT_present_timing`)

## Overview & Architectural Critique
Uneven frame presentation timing causes visual micro-stutter and judder on high-refresh-rate displays ($120\text{Hz}, 144\text{Hz}, 240\text{Hz}$), even when average frame rate exceeds 60 FPS. Standard `vkQueuePresentKHR` offers no feedback on the exact nanosecond timestamp when a frame was scanned out to the physical display panel.

In Vulkan 1.4, **Present Timing & Swapchain Feedback (`VK_EXT_present_timing`)** provides `vkGetPastPresentationTimingEXT`, returning precise hardware scanout timestamps, refresh cycle counts, and presentation latency. Applications can dynamically pace future frames by target presentation times (`VkPresentTimeGOOGLE` / `VkPresentTimesInfoGOOGLE`), eliminating micro-judder.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_present_timing` Feature**: `presentTiming = VK_TRUE`.
- **Querying Past Presentation**: `vkGetPastPresentationTimingEXT` retrieving `actualPresentTime`, `earliestPresentTime`, and `presentMarginInNanoseconds`.
- **Target Presentation Times**: Quantizing frame present requests to the exact display V-Sync refresh interval.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Query Past Presentation Timings
uint32_t count = 0;
vkGetPastPresentationTimingEXT(device, swapchain, &count, nullptr);
std::vector<VkPastPresentationTimingEXT> timings(count);
vkGetPastPresentationTimingEXT(device, swapchain, &count, timings.data());

for (const auto& timing : timings) {
    uint64_t actualPresentNs = timing.actualPresentTime;
    uint64_t presentMarginNs = timing.presentMarginInNanoseconds;
    // Calculate display refresh delta and adjust future frame pacing...
}

// 2. Schedule Future Present Time (Quantized to next V-Sync interval)
VkPresentTimeGOOGLE targetTime{
    .presentID = currentPresentId,
    .desiredPresentTime = nextVsyncTimestampNs
};

VkPresentTimesInfoGOOGLE presentTimesInfo{
    .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
    .swapchainCount = 1,
    .pTimes = &targetTime
};
// Attached into VkPresentInfoKHR.pNext during vkQueuePresentKHR
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_present_timing` on physical device.
- [x] Extract hardware presentation timings using `vkGetPastPresentationTimingEXT`.
- [x] Implement smooth frame pacing algorithm quantizing presentation requests to display refresh rate.
- [x] Eliminate frame judder and verify clean validation layer execution.

## Directory Structure
- `src/main.cpp`: Present timing frame pacing host application.
- `shaders/pacing_scene.vert`, `shaders/pacing_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
