# Assignment 46 – Zero-Allocation Push Descriptors (`VK_KHR_push_descriptor` / Vulkan 1.4 Core)

## Overview & Architectural Critique
Allocating `VkDescriptorSet` objects from `VkDescriptorPool` and updating them via `vkUpdateDescriptorSets` requires multi-threaded locking, heap allocations, and CPU cache misses.

In Vulkan 1.4, **Push Descriptors (`VK_KHR_push_descriptor` / Vulkan 1.4 Core)** allows uniform buffers, storage buffers, and image samplers to be pushed directly into the active command buffer via `vkCmdPushDescriptorSetKHR`. This eliminates `VkDescriptorPool` and `VkDescriptorSet` allocations completely for frequently updated per-pass or per-object resources.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Push Descriptors**: `VkPhysicalDevicePushDescriptorPropertiesKHR` and `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR`.
- **Direct Command Buffer Recording**: `vkCmdPushDescriptorSetKHR(cmd, pipelineBindPoint, layout, setIndex, count, pDescriptorWrites)`.
- **Zero-Allocation**: No descriptor pools or descriptor sets required.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Layout with PUSH_DESCRIPTOR flag
VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
    .bindingCount = 1,
    .pBindings = &uboBinding
};
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &pushLayout);

// 2. Record Draw with Direct Push Descriptors
VkDescriptorBufferInfo bufferInfo{
    .buffer = uniformBuffer,
    .offset = 0,
    .range = sizeof(SceneUBO)
};

VkWriteDescriptorSet writeDescriptor{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = VK_NULL_HANDLE, // Explicitly NULL for Push Descriptors!
    .dstBinding = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pBufferInfo = &bufferInfo
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

// Push descriptor directly into command buffer stream
vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &writeDescriptor);

vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Query physical device push descriptor properties.
- [x] Create `VkDescriptorSetLayout` with `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR`.
- [x] Push uniform and sampler descriptors directly via `vkCmdPushDescriptorSetKHR` without allocating any `VkDescriptorSet`.
- [x] Render animated scene with 60+ FPS and 100% clean validation layer execution.

## Directory Structure
- `src/main.cpp`: Push descriptors application source code.
- `shaders/push_desc.vert`, `shaders/push_desc.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
