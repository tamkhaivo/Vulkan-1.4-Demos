# Assignment 45 – Robustness2, Pipeline Robustness & Fault Tolerance (`VK_EXT_pipeline_robustness`)

## Overview & Architectural Critique
In complex bindless rendering engines, dynamic array indexing errors or uninitialized descriptors can cause GPU Out-of-Bounds (OOB) memory reads/writes. In standard Vulkan, OOB access results in undefined GPU behavior, driver crashes, and OS Timeout Detection and Recovery (TDR) events.

In Vulkan 1.4, **Pipeline Robustness (`VK_EXT_pipeline_robustness` / `VK_EXT_robustness2`)** allows configuring granular, per-pipeline safety guarantees without global driver overhead. Unbound or null descriptors safely return zero, and out-of-bounds vertex/storage/uniform buffer reads return deterministic zeros without crashing the GPU.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_pipeline_robustness` Feature**: `pipelineRobustness = VK_TRUE`.
- **`VK_EXT_robustness2` Features**: `nullDescriptor = VK_TRUE`, `robustBufferAccess2 = VK_TRUE`.
- **Per-Pipeline Robustness Structure**: `VkPipelineRobustnessCreateInfoEXT` embedded in `VkGraphicsPipelineCreateInfo.pNext`.
- **Safe Behavior**: Out-of-bounds reads return 0; writes are discarded safely.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Configure Per-Pipeline Robustness Guarantees
VkPipelineRobustnessCreateInfoEXT robustnessInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
    .pNext = nullptr,
    .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
    .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
    .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
    .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT
};

VkGraphicsPipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &robustnessInfo, // Embedded directly into pipeline pNext chain
    .stageCount = 2,
    .pStages = shaderStages,
    .layout = pipelineLayout,
    .renderPass = VK_NULL_HANDLE
};
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &faultTolerantPipeline);
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_pipeline_robustness` and `nullDescriptor` features.
- [x] Create graphics pipeline configured with `VkPipelineRobustnessCreateInfoEXT`.
- [x] Intentionally execute shader reading past buffer boundaries and binding `VK_NULL_HANDLE` descriptors.
- [x] Verify deterministic zero returns and confirm zero GPU device hangs / TDR crashes.

## Directory Structure
- `src/main.cpp`: Pipeline robustness host application.
- `shaders/robust_test.vert`, `shaders/robust_test.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
