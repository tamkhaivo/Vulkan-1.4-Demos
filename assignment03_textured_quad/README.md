# Assignment 3 – Textured Quad with Sampler

## Overview & Architectural Critique
Texture mapping in Vulkan involves explicit device memory allocation, optimal tiling layout (`VK_IMAGE_TILING_OPTIMAL`), mipmapping, sampler creation, and combined image sampler descriptor binding.

In Vulkan 1.4, image layout transitions are orchestrated using **Synchronization2** `VkImageMemoryBarrier2` structures. Images are initially staged via host-visible `VkBuffer`, transitioned to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` for `vkCmdCopyBufferToImage`, and then transitioned to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` with `VK_ACCESS_2_SHADER_READ_BIT` and `VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT`.

## Key Vulkan 1.4 Concepts
- **Image Creation & Allocation**: `VkImageCreateInfo` with `VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT` and device-local memory allocation.
- **Synchronization2 Staging Barriers**: `VkImageMemoryBarrier2` transitions for copy destination and shader read layout stages.
- **Sampler State**: `VkSamplerCreateInfo` with anisotropic filtering (`maxAnisotropy = 16.0f`), address mode clamp/repeat, and linear mipmap filtering.
- **Combined Image Sampler**: Binding index in descriptor set with `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` sampled in GLSL via `uniform sampler2D texSampler;`.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Upload Staged Pixel Data to Optimal Device Image with Synchronization2
void copyBufferToImageAndTransition(VkCommandBuffer cmd, VkBuffer stagingBuffer, VkImage image, uint32_t width, uint32_t height) {
    // Transition Undefined -> Transfer Dst Optimal
    VkImageMemoryBarrier2 toDstBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo depToDst{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toDstBarrier
    };
    vkCmdPipelineBarrier2(cmd, &depToDst);

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = {0, 0, 0},
        .imageExtent = { width, height, 1 }
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition Transfer Dst Optimal -> Shader Read Only Optimal
    VkImageMemoryBarrier2 toShaderBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo depToShader{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toShaderBarrier
    };
    vkCmdPipelineBarrier2(cmd, &depToShader);
}

// 2. Texture Sampler Creation
VkSamplerCreateInfo samplerInfo{
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = 16.0f,
    .compareEnable = VK_FALSE,
    .minLod = 0.0f,
    .maxLod = 0.0f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
};
vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
```

## Acceptance Criteria
- [x] Generate or load an RGBA procedural checkerboard/image texture.
- [x] Create a `VK_IMAGE_TILING_OPTIMAL` image and allocate device-local memory.
- [x] Perform staging buffer memory copies and Synchronization2 image layout transitions.
- [x] Create `VkSampler` with linear filtering and 16x anisotropic filtering.
- [x] Bind combined image sampler (`VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`) to fragment shader.
- [x] Render a textured quad with correct UV coordinate mapping and crisp bilinear filtering.

## Directory Structure
- `src/main.cpp`: Textured quad application source code.
- `shaders/textured.vert`, `shaders/textured.frag`: Texture sampling shaders.
- `CMakeLists.txt`: Build target configuration.
