# Assignment 12 – Modern Bindless with Descriptor Buffers (`VK_EXT_descriptor_buffer`)

## Overview & Architectural Critique
Vulkan's traditional descriptor architecture (`VkDescriptorPool`, `VkDescriptorSet`, `vkUpdateDescriptorSets`) imposes significant CPU memory allocation overhead, cache misses, and driver translation layers.

In Vulkan 1.4, **Descriptor Buffers (`VK_EXT_descriptor_buffer`)** replaces descriptor pools entirely. Descriptors are written directly into standard `VkBuffer` memory by querying hardware descriptor sizes and layouts (`vkGetDescriptorSetLayoutBindingOffsetEXT`, `vkGetDescriptorEXT`). Binding descriptors in command buffers becomes a single zero-allocation call: `vkCmdBindDescriptorBuffersEXT` and `vkCmdSetDescriptorBufferOffsetsEXT`.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_descriptor_buffer` Features**: `descriptorBuffer = VK_TRUE`.
- **Descriptor Buffer Properties**: Querying `descriptorBufferOffsetAlignment`, `uniformBufferDescriptorSize`, `sampledImageDescriptorSize`, and `storageBufferDescriptorSize`.
- **Layout Creation Flags**: `VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`.
- **Binding Descriptors**: `vkCmdBindDescriptorBuffersEXT` and `vkCmdSetDescriptorBufferOffsetsEXT`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Query Descriptor Sizes and Layout Offsets
VkPhysicalDeviceDescriptorBufferPropertiesEXT descBufferProps{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT
};
VkPhysicalDeviceProperties2 deviceProps{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &descBufferProps
};
vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps);

// 2. Create Layout with DESCRIPTOR_BUFFER Flag
VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
    .bindingCount = 1,
    .pBindings = &uboBinding
};
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

VkDeviceSize layoutSizeInBytes = 0;
vkGetDescriptorSetLayoutSizeEXT(device, descriptorSetLayout, &layoutSizeInBytes);

// 3. Allocate Descriptor Buffer & Write Descriptor Memory Directly
VkDeviceSize bindingOffset = 0;
vkGetDescriptorSetLayoutBindingOffsetEXT(device, descriptorSetLayout, 0, &bindingOffset);

VkDescriptorAddressInfoEXT addrInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
    .address = uboBufferDeviceAddress,
    .range = sizeof(UniformData),
    .format = VK_FORMAT_UNDEFINED
};
VkDescriptorGetInfoEXT getInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .data = { .pUniformBuffer = &addrInfo }
};

// Write descriptor directly into mapped buffer memory
char* descriptorBufferPtr = (char*)mappedDescriptorMemory;
vkGetDescriptorEXT(device, &getInfo, descBufferProps.uniformBufferDescriptorSize, descriptorBufferPtr + bindingOffset);

// 4. Command Recording: Bind Descriptor Buffer
VkDescriptorBufferBindingInfoEXT bindingInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
    .address = descriptorBufferDeviceAddress,
    .usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
};
vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

uint32_t bufferIndex = 0;
VkDeviceSize bufferOffset = 0;
vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bufferIndex, &bufferOffset);

vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
```

## Acceptance Criteria
- [x] Enable `VK_EXT_descriptor_buffer` features and retrieve hardware descriptor sizes.
- [x] Create `VkDescriptorSetLayout` with `VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`.
- [x] Allocate host-visible device-local memory for descriptor buffers (`VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT`).
- [x] Populate descriptor memory directly using `vkGetDescriptorEXT` without creating any `VkDescriptorPool`.
- [x] Bind descriptor buffers via `vkCmdBindDescriptorBuffersEXT` and set offsets via `vkCmdSetDescriptorBufferOffsetsEXT`.
- [x] Verify flawless rendering with 100% clean Vulkan validation layers.

## Directory Structure
- `src/main.cpp`: Descriptor buffers implementation source code.
- `shaders/desc_buffer.vert`, `shaders/desc_buffer.frag`: Shaders accessed via descriptor buffer.
- `CMakeLists.txt`: Build target configuration.
