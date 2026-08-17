# Assignment 29 – Host Image Copy & Direct Host Uploads (`VK_EXT_host_image_copy`)

## Overview & Architectural Critique
In standard Vulkan texture loading, updating an image requires allocating a host-visible staging `VkBuffer`, copying CPU memory into the buffer, allocating and recording a `VkCommandBuffer`, inserting image layout transition pipeline barriers, submitting the command buffer to a queue, and waiting on a fence.

In Vulkan 1.4, **Host Image Copy (`VK_EXT_host_image_copy` / Vulkan 1.4 Core)** introduces direct host CPU-to-image memory copies (`vkCopyMemoryToImageEXT`) and host-side layout transitions (`vkTransitionImageLayoutEXT`), completely bypassing staging buffers, command pools, and queue submissions for asset streaming.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_host_image_copy` Feature**: `hostImageCopy = VK_TRUE`.
- **Image Usage Flags**: `VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT`.
- **Host-Side Image Layout Transitions**: `vkTransitionImageLayoutEXT` moving images from `VK_IMAGE_LAYOUT_UNDEFINED` to `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` or `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` directly on the CPU.
- **Direct Memory Copy**: `vkCopyMemoryToImageEXT` streaming raw host pointers directly into optimal GPU images.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Image with HOST_TRANSFER Usage
VkImageCreateInfo imageInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { width, height, 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT
};
vkCreateImage(device, &imageInfo, nullptr, &textureImage);
// (Allocate and bind device memory)

// 2. Perform Host-Side Layout Transition (No Command Buffer, No GPU Queue)
VkHostImageLayoutTransitionInfoEXT hostTransition{
    .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
    .image = textureImage,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
};
vkTransitionImageLayoutEXT(device, 1, &hostTransition);

// 3. Direct CPU-to-Image Copy (Zero Staging Buffer)
VkMemoryToImageCopyEXT copyRegion{
    .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
    .pHostPointer = rawPixelDataPointer, // Direct CPU pointer
    .memoryRowLength = 0,
    .memoryImageHeight = 0,
    .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
    .imageOffset = {0, 0, 0},
    .imageExtent = { width, height, 1 }
};

VkCopyMemoryToImageInfoEXT copyInfo{
    .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
    .flags = 0,
    .dstImage = textureImage,
    .dstImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    .regionCount = 1,
    .pRegions = &copyRegion
};

vkCopyMemoryToImageEXT(device, &copyInfo); // Immediate CPU-to-GPU write
```

## Acceptance Criteria
- [x] Query and enable `VK_EXT_host_image_copy` physical device features.
- [x] Create images with `VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT`.
- [x] Transition image layouts using `vkTransitionImageLayoutEXT` entirely on the host CPU.
- [x] Upload texture data directly via `vkCopyMemoryToImageEXT` without staging `VkBuffer` or command recording.
- [x] Sample the uploaded texture in graphics passes with 100% clean validation layer execution.

## Directory Structure
- `src/main.cpp`: Host image copy application source code.
- `shaders/host_copy.vert`, `shaders/host_copy.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
