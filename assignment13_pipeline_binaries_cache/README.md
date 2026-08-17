# Assignment 13 – Pipeline Binaries & Cache Optimization (`VK_KHR_pipeline_binary` / `VkPipelineCache`)

## Overview & Architectural Critique
Shader compilation during gameplay causes severe runtime frame hitches and stutter. While `VkPipelineCache` provides basic caching, compilation is still vendor-dependent and non-deterministic.

In Vulkan 1.4, **Pipeline Binaries (`VK_KHR_pipeline_binary`)** and enhanced pipeline caches allow applications to extract fully pre-compiled GPU hardware binaries (`vkGetPipelineBinaryDataKHR`), serialize them to persistent disk storage, and pre-warm pipeline state objects (PSOs) at launch with near-zero initialization latency.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_pipeline_binary` Features**: `pipelineBinaries = VK_TRUE`.
- **Pipeline Cache (`VkPipelineCache`)**: Serializing cache blobs via `vkGetPipelineCacheData` and reloading on application startup.
- **Pipeline Binary Querying**: `vkCreatePipelineBinariesKHR` and `vkGetPipelineBinaryDataKHR`.
- **Pre-Warming**: Asynchronous background pipeline compilation threads loading cached binaries.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Load Serialized Pipeline Cache from Disk
std::vector<char> cacheData = readBinaryFile("pipeline_cache.bin");

VkPipelineCacheCreateInfo cacheCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    .initialDataSize = cacheData.size(),
    .pInitialData = cacheData.empty() ? nullptr : cacheData.data()
};
VkPipelineCache pipelineCache;
vkCreatePipelineCache(device, &cacheCreateInfo, nullptr, &pipelineCache);

// 2. Create Graphics Pipeline with Cache
VkGraphicsPipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &renderingCreateInfo,
    .stageCount = 2,
    .pStages = shaderStages,
    .layout = pipelineLayout,
    .renderPass = VK_NULL_HANDLE
};
vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &graphicsPipeline);

// 3. Serialize Updated Pipeline Cache back to Disk
size_t cacheSize = 0;
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr);

std::vector<char> serializedCache(cacheSize);
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, serializedCache.data());
writeBinaryFile("pipeline_cache.bin", serializedCache);
```

## Acceptance Criteria
- [x] Implement robust disk file serialization and deserialization for `VkPipelineCache`.
- [x] Create `VkPipelineCache` with initial binary blob on startup.
- [x] Create multiple graphics/compute pipelines using the initialized cache.
- [x] Query updated cache size and write the compiled cache blob to disk on shutdown.
- [x] Measure and log startup pipeline compilation time reduction (confirming 90%+ time savings on subsequent runs).

## Directory Structure
- `src/main.cpp`: Pipeline cache serialization and loading application.
- `shaders/cached_scene.vert`, `shaders/cached_scene.frag`: Target shaders.
- `CMakeLists.txt`: Build target configuration.
