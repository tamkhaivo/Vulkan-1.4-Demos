# Assignment 5 – Instanced Rendering with Vertex Attribute Divisor

## Overview & Architectural Critique
Issuing separate draw calls for thousands of identical or similar geometric meshes introduces massive CPU driver submission overhead. **Instanced Rendering** allows rendering $N$ instances in a single draw command (`vkCmdDrawIndexed` with `instanceCount = N`).

In Vulkan 1.4 core (and via `VK_KHR_vertex_attribute_divisor`), developers can configure per-instance vertex buffer bindings with a custom divisor. For example, a divisor of $1$ steps to the next instance data per instance, while a divisor of $N$ allows sharing instance data across $N$ primitives (useful for layered or multi-LOD architectures).

## Key Vulkan 1.4 Concepts
- **Vertex Input Binding Divisor**: `VkVertexInputBindingDivisorDescriptionKHR` embedded in `VkPipelineVertexInputDivisorStateCreateInfoKHR` (promoted to Vulkan 1.4 core `VkPhysicalDeviceVulkan14Features.vertexAttributeInstanceRateDivisor`).
- **Multiple Vertex Buffer Bindings**: Binding Binding 0 for per-vertex attributes (`VK_VERTEX_INPUT_RATE_VERTEX`) and Binding 1 for per-instance matrices/colors (`VK_VERTEX_INPUT_RATE_INSTANCE`).
- **Instanced Draw Call**: `vkCmdDrawIndexed(cmd, indexCount, instanceCount, 0, 0, 0)`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Instance Data Structure
struct InstanceData {
    glm::vec3 position;
    float scale;
    glm::vec4 color;
};

// 2. Vertex Input Setup with Divisor State
VkVertexInputBindingDescription bindingDescriptions[2] = {
    { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
    { .binding = 1, .stride = sizeof(InstanceData), .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE }
};

VkVertexInputAttributeDescription attributeDescriptions[5] = {
    // Per-vertex: pos, normal
    { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos) },
    { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
    // Per-instance: pos, scale, color
    { .location = 2, .binding = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(InstanceData, position) },
    { .location = 3, .binding = 1, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(InstanceData, scale) },
    { .location = 4, .binding = 1, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(InstanceData, color) }
};

VkVertexInputBindingDivisorDescriptionKHR divisorDesc{
    .binding = 1,
    .divisor = 1 // Step 1 element per instance
};

VkPipelineVertexInputDivisorStateCreateInfoKHR divisorState{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .bindingDivisorCount = 1,
    .pBindingDivisors = &divisorDesc
};

VkPipelineVertexInputStateCreateInfo vertexInputInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = &divisorState,
    .vertexBindingDescriptionCount = 2,
    .pVertexBindingDescriptions = bindingDescriptions,
    .vertexAttributeDescriptionCount = 5,
    .pVertexAttributeDescriptions = attributeDescriptions
};

// 3. Recording Instanced Draw
VkBuffer vertexBuffers[] = { meshVertexBuffer, instanceBuffer };
VkDeviceSize offsets[] = { 0, 0 };
vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);
vkCmdBindIndexBuffer(cmd, meshIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
vkCmdDrawIndexed(cmd, indexCount, 10000 /* 10,000 instances */, 0, 0, 0);
```

## Acceptance Criteria
- [x] Configure dual vertex buffer input bindings with per-vertex and per-instance rates.
- [x] Enable and verify `VkPipelineVertexInputDivisorStateCreateInfoKHR` in the vertex input pNext chain.
- [x] Populate dynamic instance buffer with 10,000+ animated particle/mesh transforms.
- [x] Render all 10,000 instances in a single `vkCmdDrawIndexed` invocation at 60+ FPS.
- [x] Ensure vertex shader unpacks instance positions and colors accurately without memory out-of-bounds.

## Directory Structure
- `src/main.cpp`: Instanced rendering application source code.
- `shaders/instanced.vert`, `shaders/instanced.frag`: Instanced vertex processing shaders.
- `CMakeLists.txt`: Build target configuration.
