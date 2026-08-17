# Assignment 13 – Pipeline Binaries & Cache Optimization (`VK_KHR_pipeline_binary` / `VkPipelineCache`)

## Overview
Implement zero-hitch pipeline compilation, pre-warmed PSO blobs, and on-disk pipeline cache serialization/deserialization using Vulkan pipeline cache and pipeline binary features.

## Key Concepts
- `VkPipelineCache` creation, disk serialization, and warm startup loading.
- `VK_KHR_pipeline_binary` / `VkPipelineBinaryKHR` for binary shader payload querying and direct driver blob caching.
- Eliminating runtime shader compilation stuttering by pre-warming pipeline states.
- Cache validation headers (`VkPipelineCacheHeaderVersionOne`) and driver UUID matching.

## Acceptance Criteria
- [x] Initialize `VkPipelineCache` from existing on-disk cache file (`pipeline_cache.bin`) if present.
- [x] Compile graphics pipelines associating the active `VkPipelineCache`.
- [x] Extract serialized pipeline cache data via `vkGetPipelineCacheData`.
- [x] Write updated pipeline cache payload back to disk on clean application shutdown.
- [x] Support Vulkan 1.4 / `VK_KHR_pipeline_binary` mechanisms when available.

## Directory Structure
- `src/main.cpp`: Pipeline caching & binaries Vulkan 1.4 host application.
- `shaders/`: GLSL shaders for pre-warmed pipeline creation.
- `CMakeLists.txt`: Build target configuration.
