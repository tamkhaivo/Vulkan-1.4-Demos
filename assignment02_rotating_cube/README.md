# Assignment 2 – Rotating Cube with Uniform Buffers

## Overview & Architectural Critique
Rendering 3D geometry requires mathematical transformations across Model, View, and Projection (MVP) spaces. In early Vulkan versions, uniform updates often suffered from write hazards and CPU stalls when updating buffers mapped directly to active frames in flight.

In Vulkan 1.4, Uniform Buffer Objects (UBOs) must be synchronized per frame-in-flight (e.g., double or triple buffering) using `VkBuffer` with `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` memory or device-local memory updated via staging buffers. Descriptor sets are allocated from `VkDescriptorPool` and bound to the graphics pipeline via `vkCmdBindDescriptorSets`.

## Key Vulkan 1.4 Concepts
- **Uniform Buffer Object (UBO)**: Struct layout matching `std140` alignment rules (16-byte alignment for `vec4` and `mat4`).
- **Descriptor Set Layout**: `VkDescriptorSetLayoutBinding` with `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` for `VK_SHADER_STAGE_VERTEX_BIT`.
- **Depth Buffering with Dynamic Rendering**: Inclusion of `depthAttachmentFormat = VK_FORMAT_D32_SFLOAT` in `VkPipelineRenderingCreateInfo` and `VkRenderingAttachmentInfo` for depth testing.
- **Double Buffering per Frame**: Independent UBOs and descriptor sets per swapchain image/frame in flight to eliminate CPU-GPU write contentions.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Uniform Buffer Object struct (std140 layout compliance)
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// 2. Descriptor Set Layout Binding
VkDescriptorSetLayoutBinding uboLayoutBinding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .pImmutableSamplers = nullptr
};
VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &uboLayoutBinding
};
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

// 3. Updating Uniform Buffers each frame
void updateUniformBuffer(uint32_t currentImage, float time, VkExtent2D swapExtent) {
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapExtent.width / (float)swapExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1.0f; // Invert Y for Vulkan NDC

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

// 4. Dynamic Rendering with Color and Depth Attachments
VkRenderingAttachmentInfo colorAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = swapchainImageViews[currentImage],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {{{ 0.1f, 0.1f, 0.15f, 1.0f }}}
};

VkRenderingAttachmentInfo depthAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = depthImageView,
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {.depthStencil = { 1.0f, 0 }}
};

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, swapExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment,
    .pDepthAttachment = &depthAttachment
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentImage], 0, nullptr);
vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
vkCmdEndRendering(cmd);
```

## Acceptance Criteria
- [x] Implement a 3D Cube vertex array with position and color attributes and an index buffer.
- [x] Allocate host-visible persistently mapped uniform buffers per frame in flight.
- [x] Configure descriptor pool and allocate descriptor sets with `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`.
- [x] Create a depth buffer attachment (`VK_FORMAT_D32_SFLOAT` or `VK_FORMAT_D24_UNORM_S8_UINT`) and enable depth testing/writing.
- [x] Update Model-View-Projection matrices every frame with proper Vulkan coordinate conventions (`proj[1][1] *= -1.0f`).
- [x] Render a smooth 60+ FPS rotating 3D cube with zero depth sorting artifacts or validation warnings.

## Directory Structure
- `src/main.cpp`: Rotating cube UBO application source code.
- `shaders/cube.vert`, `shaders/cube.frag`: 3D transform and shading shaders.
- `CMakeLists.txt`: Build target configuration.
