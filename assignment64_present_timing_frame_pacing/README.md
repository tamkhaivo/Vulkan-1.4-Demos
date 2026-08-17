# Assignment 64 – Advanced Frame Pacing, Present Timing & Swapchain Feedback (`VK_EXT_present_timing`)

## Overview
Implement a high-precision frame pacing and display synchronization engine using hardware present timing query extensions. Accurately measure GPU presentation latencies, refresh cycle target alignment, refresh interval offsets, and missed vs. met display v-sync deadlines.

## Key Concepts
- Present timing querying via `VK_EXT_present_timing` / `VK_GOOGLE_display_timing`.
- Frame pacing algorithms calculating target presentation times $T_{target}$.
- V-Sync phase locking and refresh cycle quantization.
- Eliminating display stutter, micro-judder, and presentation queue starvation.

## Acceptance Criteria
- [x] Query and initialize display/present timing extension support during swapchain creation.
- [x] Collect present timing records after each presented frame.
- [x] Implement an adaptive presentation scheduler targeting future V-Sync refresh intervals.
- [x] Compute latency metrics: render-to-present duration, display latency, and dropped vs. early frames.
- [x] Verify smooth frame presentation pacing and stable millisecond timing without swapchain deadlocks.

## Directory Structure
- `src/main.cpp`: Present timing and frame pacing host application.
- `CMakeLists.txt`: Build target configuration.
