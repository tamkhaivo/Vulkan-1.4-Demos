# Assignment 4 – Push Constants and Dynamic Uniform Buffers

## Overview & Architectural Critique
When rendering scenes with multiple objects sharing the same material or pipeline, allocating separate descriptor sets per object causes severe CPU overhead, descriptor pool fragmentation, and frequent pipeline rebinding.

Vulkan 1.4 provides two high-performance mechanisms for low-latency per-object data:
1. **Push Constants**: Directly written into GPU registers during command recording (`vkCmdPushConstants`). Minimum guaranteed budget is 128 bytes (sufficient for standard 64-byte `mat4` and auxiliary material flags).
2. **Dynamic Uniform Buffers (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`)**: A single large uniform buffer packed with per-object data, bound once, and indexed per-draw with a dynamic byte offset via `vkCmdBindDescriptorSets` (must respect `minUniformBufferOffsetAlignment`).

## Key Vulkan 1.4 Concepts
- **`VkPushConstantRange`**: Specifying offset, size, and shader stage visibility (`VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT`) in `VkPipelineLayoutCreateInfo`.
- **Dynamic Descriptor Binding**: Specifying `pDynamicOffsets` array during `vkCmdBindDescriptorSets` with byte offsets aligned to `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment`.
- **Zero CPU Allocation Overhead**: Eliminates `vkAllocateDescriptorSets` during per-object render loops.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Define Push Constant Structure (Max 128 bytes guaranteed)
struct PushConstants {
    glm::mat4 model;
    glm::vec4 colorTint;
};

// 2. Configure Pipeline Layout with Push Constants and Dynamic UBO
VkPushConstantRange pushConstantRange{
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .offset = 0,
    .size = sizeof(PushConstants)
};

VkDescriptorSetLayoutBinding dynamicUboBinding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
};
VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &dynamicUboBinding
};
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

VkPipelineLayoutCreateInfo pipelineLayoutInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descriptorSetLayout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pushConstantRange
};
vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

// 3. Multi-Object Draw Loop with Push Constants and Dynamic Offset Binding
const size_t uboAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
const size_t alignedSize = (sizeof(GlobalUBO) + uboAlignment - 1) & ~(uboAlignment - 1);

for (uint32_t i = 0; i < numObjects; ++i) {
    uint32_t dynamicOffset = static_cast<uint32_t>(i * alignedSize);
    
    // Bind dynamic uniform buffer offset
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, 
                            &dynamicDescriptorSet, 1, &dynamicOffset);

    // Push per-object constants
    PushConstants pc{};
    pc.model = objects[i].transform;
    pc.colorTint = objects[i].color;
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                       0, sizeof(PushConstants), &pc);

    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}
```

## Acceptance Criteria
- [x] Query and enforce `minUniformBufferOffsetAlignment` when calculating buffer slice offsets.
- [x] Configure `VkPushConstantRange` for vertex/fragment stages and set up pipeline layout.
- [x] Allocate a single consolidated dynamic uniform buffer holding view/projection matrices and per-object parameters.
- [x] Stream per-object transforms and color tints via `vkCmdPushConstants` and `vkCmdBindDescriptorSets` with dynamic offsets.
- [x] Render multiple dynamic objects (e.g. 50+ animated cubes/spheres) in a single draw loop without descriptor re-allocations.

## Directory Structure
- `src/main.cpp`: Push constants & dynamic UBO application source code.
- `shaders/push_dynamic.vert`, `shaders/push_dynamic.frag`: Multi-object shaders.
- `CMakeLists.txt`: Build target configuration.
