# Assignment 1 – Hello Triangle (Dynamic Rendering)

## Overview & Architectural Critique
In Vulkan 1.4, the traditional render pass architecture (`VkRenderPass` and `VkFramebuffer`) is completely superseded by **Dynamic Rendering** (promoted to core in 1.3/1.4). Legacy render passes required monolithic state tracking, pre-declaring attachment load/store operations, subpass dependencies, and rigid framebuffer allocations bound to exact image views ahead of time.

Dynamic rendering simplifies the rendering pipeline by moving attachment binding directly into the command buffer recording stage via `vkCmdBeginRendering` and `vkCmdEndRendering`. When implementing dynamic rendering, developers must carefully manage explicit image layout transitions using Vulkan 1.4 core **Synchronization2** (`VkImageMemoryBarrier2`), as implicit subpass layout transitions are no longer performed by the driver.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Dynamic Rendering**: Device feature `dynamicRendering = VK_TRUE` (enabled by default in Vulkan 1.4).
- **Pipeline Setup**: `VkPipelineRenderingCreateInfo` embedded inside `VkGraphicsPipelineCreateInfo.pNext` specifying `colorAttachmentCount` and color format arrays without `VkRenderPass`.
- **Synchronization2 Layout Transitions**: Utilizing `vkCmdPipelineBarrier2` with `VkDependencyInfo` and `VkImageMemoryBarrier2` to transition swapchain images from `VK_IMAGE_LAYOUT_UNDEFINED` to `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`, and finally to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`.
- **Inline Rendering Scope**: Recording `vkCmdBeginRendering` with `VkRenderingInfo` containing `VkRenderingAttachmentInfo` color/depth attachment targets.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Pipeline Creation with Dynamic Rendering PNext Chain
VkPipelineRenderingCreateInfo renderingCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .pNext = nullptr,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &swapchainImageFormat, // e.g. VK_FORMAT_B8G8R8A8_SRGB
    .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
};

VkGraphicsPipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &renderingCreateInfo,
    .stageCount = 2,
    .pStages = shaderStages,
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampling,
    .pColorBlendState = &colorBlending,
    .pDynamicState = &dynamicState,
    .layout = pipelineLayout,
    .renderPass = VK_NULL_HANDLE // Explicitly null in Vulkan 1.4 Dynamic Rendering
};
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

// 2. Command Buffer Recording with Synchronization2 and vkCmdBeginRendering
VkImageMemoryBarrier2 barrierToColor{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .image = swapchainImages[imageIndex],
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
};
VkDependencyInfo depInfoToColor{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrierToColor
};
vkCmdPipelineBarrier2(cmd, &depInfoToColor);

VkRenderingAttachmentInfo colorAttachment{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = swapchainImageViews[imageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {{{ 0.05f, 0.05f, 0.05f, 1.0f }}}
};

VkRenderingInfo renderInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = { {0, 0}, swapchainExtent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachment
};

vkCmdBeginRendering(cmd, &renderInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
vkCmdDraw(cmd, 3, 1, 0, 0);
vkCmdEndRendering(cmd);

// Transition to Present
VkImageMemoryBarrier2 barrierToPresent{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .image = swapchainImages[imageIndex],
    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
};
VkDependencyInfo depInfoToPresent{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrierToPresent
};
vkCmdPipelineBarrier2(cmd, &depInfoToPresent);
```

## Acceptance Criteria
- [x] Initialize Vulkan 1.4 physical device with `VkPhysicalDeviceVulkan13Features.dynamicRendering = VK_TRUE` and `synchronization2 = VK_TRUE`.
- [x] Create GLFW window surface and configure `VkSwapchainKHR` targeting `VK_FORMAT_B8G8R8A8_SRGB` or `VK_FORMAT_R8G8B8A8_SRGB`.
- [x] Construct `VkPipelineRenderingCreateInfo` in the `VkGraphicsPipelineCreateInfo.pNext` chain with `renderPass = VK_NULL_HANDLE`.
- [x] Execute explicit layout transitions via `vkCmdPipelineBarrier2` (`VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` -> `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`).
- [x] Record render passes using `vkCmdBeginRendering` with `VK_ATTACHMENT_LOAD_OP_CLEAR` and `vkCmdEndRendering`.
- [x] Render RGB vertex interpolated triangle at 60+ FPS with zero validation layer warnings or errors.

## Directory Structure
- `src/main.cpp`: Main dynamic rendering application source code.
- `shaders/triangle.vert`, `shaders/triangle.frag`: SPIR-V vertex and fragment shaders.
- `CMakeLists.txt`: Build target configuration.
