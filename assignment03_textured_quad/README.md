# Assignment 3 – Textured Quad with Sampler

## Overview
Load a texture from disk, upload to GPU memory, transition image layouts, and sample in the fragment shader.

## Key Concepts
- `VkImage` creation, allocation, staging buffer copy.
- Image layout transitions (`UNDEFINED` -> `TRANSFER_DST_OPTIMAL` -> `SHADER_READ_ONLY_OPTIMAL`) via `vkCmdPipelineBarrier`.
- `VkImageView` and `VkSampler` configuration.
- `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`.
