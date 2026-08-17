# Assignment 12 – Modern Bindless with Descriptor Buffers (`VK_EXT_descriptor_buffer`)

## Overview
Replace legacy descriptor pools and descriptor sets with direct GPU memory writes into uniform/storage descriptor buffers via `VK_EXT_descriptor_buffer`.

## Key Concepts
- `VK_EXT_descriptor_buffer` feature enablement (`VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer`).
- Descriptor buffer memory allocation with `VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT` and `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`.
- Direct layout size and byte offset querying using `vkGetDescriptorSetLayoutSizeEXT` and `vkGetDescriptorSetLayoutBindingOffsetEXT`.
- Writing raw descriptor payloads directly into mapped memory via `vkGetDescriptorEXT`.
- Binding descriptor buffers via `vkCmdBindDescriptorBuffersEXT` and setting offsets via `vkCmdSetDescriptorBufferOffsetsEXT`.

## Acceptance Criteria
- [x] Enable `descriptorBuffer` in `VkPhysicalDeviceDescriptorBufferFeaturesEXT`.
- [x] Query and satisfy descriptor buffer properties and alignments (`VkPhysicalDeviceDescriptorBufferPropertiesEXT`).
- [x] Create resource and sampler descriptor buffers with appropriate BDA and descriptor buffer usage bits.
- [x] Obtain descriptor data with `vkGetDescriptorEXT` and write directly to mapped memory without `VkDescriptorPool` / `VkDescriptorSet`.
- [x] Bind descriptor buffers during command buffer recording with `vkCmdBindDescriptorBuffersEXT` and `vkCmdSetDescriptorBufferOffsetsEXT`.

## Directory Structure
- `src/main.cpp`: Descriptor buffers Vulkan 1.4 host application.
- `shaders/`: GLSL shaders consuming descriptor buffer resources.
- `CMakeLists.txt`: Build target configuration.
