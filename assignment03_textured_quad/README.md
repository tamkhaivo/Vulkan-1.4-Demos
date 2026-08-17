# Assignment 3 – Textured Quad with Sampler

## Overview
Generate or load texture image data, allocate GPU device memory, execute staging copy buffers with Vulkan 1.4 Synchronization2 pipeline barriers, create samplers, and sample textures on a 3D geometry surface in fragment shaders.

## Key Concepts
- `VkImage` and `VkDeviceMemory` allocation for 2D color textures (`VK_FORMAT_R8G8B8A8_UNORM`).
- Staging buffer to `VkImage` data transfer (`vkCmdCopyBufferToImage`).
- Vulkan 1.4 image layout transitions via `VkPipelineBarrier2` / `VkImageMemoryBarrier2` (`UNDEFINED` -> `TRANSFER_DST_OPTIMAL` -> `SHADER_READ_ONLY_OPTIMAL`).
- `VkImageView` creation and configurable `VkSampler` configuration (linear filtering, address modes, anisotropy).
- Descriptor binding using `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`.
- Perspective textured rendering with UV coordinate mapping.

## Acceptance Criteria
- [x] Generate procedural or load image pixel data into host-visible staging memory.
- [x] Create device-local `VkImage` and transition its layout for transfer destination using `vkCmdPipelineBarrier2`.
- [x] Copy pixel buffer data to the image with `vkCmdCopyBufferToImage` and transition image layout to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
- [x] Create `VkImageView` and `VkSampler` with linear filtering and clamp/repeat address modes.
- [x] Configure `VkDescriptorSetLayout` and allocate descriptor set binding both MVP Uniform Buffer and Combined Image Sampler.
- [x] Sample the texture in fragment shader (`texture(texSampler, inTexCoord)`) and render quad with depth test.

## Directory Structure
- `src/main.cpp`: Main application source code.
- `shaders/`: GLSL shaders (`quad.vert`, `quad.frag`).
- `CMakeLists.txt`: Build target configuration.
