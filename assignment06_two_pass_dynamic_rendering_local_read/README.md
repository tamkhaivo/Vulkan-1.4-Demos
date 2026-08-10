# Assignment 6 – Two-Pass Effect with Dynamic Rendering Local Reads

## Overview
Implement a post-processing blur effect: render a 3D scene to an offscreen color attachment, then perform on-chip local reads using Vulkan 1.4's dynamic rendering local read feature.

## Key Concepts
- `VK_KHR_dynamic_rendering_local_read` (Vulkan 1.4 core).
- `VkRenderingInputAttachmentInfoKHR` chained via `pNext` in `VkRenderingInfo`.
- Shader `subpassInput` reading without legacy subpass dependencies or `VkRenderPass` objects.
