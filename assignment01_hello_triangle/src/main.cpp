// ============================================================================
// Assignment 1: Hello Triangle (Vulkan 1.4 Core Dynamic Rendering)
// ============================================================================
// Core Vulkan 1.4 Concepts Demonstrated:
// 1. Vulkan 1.4 Instance & Device Initialization targeting VK_API_VERSION_1_4.
// 2. Native Core Features (VkPhysicalDeviceVulkan13Features / Vulkan14Features):
//    - Dynamic Rendering (vkCmdBeginRendering / VkRenderingInfo without VkRenderPass)
//    - Synchronization2 (vkCmdPipelineBarrier2 / VkImageMemoryBarrier2 / VkDependencyInfo)
// 3. Swapchain Setup (`VkSwapchainKHR`) with color image views.
// 4. Graphics Pipeline Creation using native core `VkPipelineRenderingCreateInfo`.
// 5. Explicit CPU-GPU Frame Synchronization using Fences and Semaphores.
// ============================================================================

#include <vulkan/vulkan.h>     // Core Vulkan 1.4 API definitions and structures
#include <GLFW/glfw3.h>        // GLFW for cross-platform windowing and surface creation
#include <iostream>            // Standard stream I/O for log output
#include <vector>              // Standard dynamic array container
#include <string>              // Standard string type for paths
#include <filesystem>          // C++17 filesystem library for verifying shader presence
#include "vulkan_common.hpp"   // Helper definitions and convenience utilities (vulkan_utils & vk_common)

// Define Vulkan 1.4 core function pointer types for dynamic invocation
typedef void (VKAPI_PTR *PFN_vkCmdBeginRenderingCore)(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo);
typedef void (VKAPI_PTR *PFN_vkCmdEndRenderingCore)(VkCommandBuffer commandBuffer);
typedef void (VKAPI_PTR *PFN_vkCmdPipelineBarrier2Core)(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo);

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 1: Hello Triangle (Vulkan 1.4 Core Specification)" << std::endl;
    std::cout << "Targeting VK_API_VERSION_1_4 with Dynamic Rendering & Synchronization2" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        // Define default window dimensions in pixels
        const uint32_t WIDTH = 800;   // Render window width
        const uint32_t HEIGHT = 600;  // Render window height

        // STEP 1: Create GLFW Window configured for Vulkan (GLFW_NO_API)
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 1: Hello Triangle (Vulkan 1.4 Core)");

        // STEP 2: Create Vulkan 1.4 Instance (vkCreateInstance targeting VK_API_VERSION_1_4)
        VkInstance instance = vulkan_utils::createInstance();

        // STEP 3: Create OS Window Surface (glfwCreateWindowSurface)
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);

        // STEP 4: Select Suitable Physical Device (Discrete GPU priority)
        VkPhysicalDevice physical_device = vulkan_utils::findPhysicalDevice(instance);

        // STEP 5: Find Queue Family Supporting Graphics Operations & Surface Presentation
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                graphicsQueueFamily = i;
                break;
            }
        }

        if (graphicsQueueFamily == UINT32_MAX) {
            throw std::runtime_error("Could not find a queue family supporting both graphics and presentation!");
        }

        // STEP 6: Configure Vulkan 1.4 Logical Device Creation Info
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        // Vulkan 1.4 device extensions (VK_KHR_swapchain for presentation)
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // Enable Vulkan 1.4 Native Core Features (Dynamic Rendering & Synchronization2)
        // In Vulkan 1.4, these features are core and enabled via VkPhysicalDeviceVulkan13Features / Vulkan14Features!
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE; // Native Dynamic Rendering (No VkRenderPass)
        vulkan13Features.synchronization2 = VK_TRUE; // Native Synchronization2 (vkCmdPipelineBarrier2 / VkImageMemoryBarrier2)

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features; // Chain Vulkan 1.3 / 1.4 core feature structure

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Instantiate Logical Device
        VkDevice device;
        vk_common::check_vk_result(vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device), "Failed to create Vulkan 1.4 logical device");

        // Retrieve handle to the created graphics queue
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        // STEP 7: Load Vulkan 1.4 Core API Function Pointers
        auto vkCmdBeginRenderingFunc = (PFN_vkCmdBeginRenderingCore)vkGetDeviceProcAddr(device, "vkCmdBeginRendering");
        if (!vkCmdBeginRenderingFunc) {
            vkCmdBeginRenderingFunc = (PFN_vkCmdBeginRenderingCore)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
        }

        auto vkCmdEndRenderingFunc = (PFN_vkCmdEndRenderingCore)vkGetDeviceProcAddr(device, "vkCmdEndRendering");
        if (!vkCmdEndRenderingFunc) {
            vkCmdEndRenderingFunc = (PFN_vkCmdEndRenderingCore)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
        }

        auto vkCmdPipelineBarrier2Func = (PFN_vkCmdPipelineBarrier2Core)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2");
        if (!vkCmdPipelineBarrier2Func) {
            vkCmdPipelineBarrier2Func = (PFN_vkCmdPipelineBarrier2Core)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2KHR");
        }

        if (!vkCmdBeginRenderingFunc || !vkCmdEndRenderingFunc || !vkCmdPipelineBarrier2Func) {
            throw std::runtime_error("Could not load Vulkan 1.4 core function pointers!");
        }

        // STEP 8: Create Swapchain (`VkSwapchainKHR`)
        VkSurfaceFormatKHR surfaceFormat{VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR};

        VkSwapchainCreateInfoKHR swapchain_create_info{};
        swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface = surface;
        swapchain_create_info.minImageCount = 2; // Double buffering
        swapchain_create_info.imageFormat = surfaceFormat.format;
        swapchain_create_info.imageColorSpace = surfaceFormat.colorSpace;
        swapchain_create_info.imageExtent = {WIDTH, HEIGHT};
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync enabled
        swapchain_create_info.clipped = VK_TRUE;
        swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain;
        vk_common::check_vk_result(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain), "Failed to create swapchain");

        // Retrieve Swapchain Image handles
        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        // STEP 9: Create Image Views for each Swapchain Image
        std::vector<VkImageView> swapchainImageViews(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain image view");
        }

        // STEP 10: Load Shader Bytecode (SPIR-V)
        std::string vertPath = "shaders/triangle.vert.spv";
        std::string fragPath = "shaders/triangle.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment01_hello_triangle/shaders/triangle.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment01_hello_triangle/shaders/triangle.frag.spv";

        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

        VkShaderModule vertShaderModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragShaderModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // STEP 11: Configure Pipeline Fixed-Function Stages
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 12: Vulkan 1.4 Native Pipeline Dynamic Rendering Configuration (`VkPipelineRenderingCreateInfo`)
        VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
        pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO; // Core Vulkan 1.3 / 1.4 structure type
        pipeline_rendering_create_info.colorAttachmentCount = 1;
        pipeline_rendering_create_info.pColorAttachmentFormats = &surfaceFormat.format;

        // STEP 13: Create Graphics Pipeline with Dynamic Rendering
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipeline_rendering_create_info; // Attach native VkPipelineRenderingCreateInfo
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = VK_NULL_HANDLE; // No VkRenderPass used in Vulkan 1.4 Dynamic Rendering!

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 14: Allocate Command Pool and Command Buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "Failed to allocate command buffer");

        // STEP 15: Create CPU-GPU Synchronization Objects
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence fence;
        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateFence(device, &fence_create_info, nullptr, &fence), "Failed to create fence");

        std::cout << "Vulkan 1.4 Setup completed successfully. Entering main rendering loop..." << std::endl;

        // ====================================================================
        // STEP 16: Main Vulkan 1.4 Rendering Loop
        // ====================================================================
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // 1. Wait for fence from previous frame
            vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &fence);

            // 2. Acquire Next Image from Swapchain
            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Failed to acquire swapchain image!");
            }

            // 3. Reset and Begin Command Buffer Recording
            vkResetCommandBuffer(commandBuffer, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_common::check_vk_result(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer");

            // 4. Vulkan 1.4 Native Synchronization2 Image Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
            VkImageMemoryBarrier2 barrierToAttachment{};
            barrierToAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2; // Core Vulkan 1.4 Synchronization2 Barrier
            barrierToAttachment.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrierToAttachment.srcAccessMask = VK_ACCESS_2_NONE;
            barrierToAttachment.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToAttachment.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToAttachment.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierToAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierToAttachment.image = swapchainImages[imageIndex];
            barrierToAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrierToAttachment.subresourceRange.baseMipLevel = 0;
            barrierToAttachment.subresourceRange.levelCount = 1;
            barrierToAttachment.subresourceRange.baseArrayLayer = 0;
            barrierToAttachment.subresourceRange.layerCount = 1;

            VkDependencyInfo dependencyInfoAttachment{};
            dependencyInfoAttachment.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfoAttachment.imageMemoryBarrierCount = 1;
            dependencyInfoAttachment.pImageMemoryBarriers = &barrierToAttachment;

            // Execute Vulkan 1.4 Synchronization2 Pipeline Barrier
            vkCmdPipelineBarrier2Func(commandBuffer, &dependencyInfoAttachment);

            // 5. Configure Core Dynamic Rendering Attachment (`VkRenderingAttachmentInfo`)
            VkRenderingAttachmentInfo color_attachment{};
            color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO; // Core Vulkan 1.3 / 1.4 structure type
            color_attachment.imageView = swapchainImageViews[imageIndex];
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.clearValue.color = {{0.1f, 0.1f, 0.15f, 1.0f}}; // Clear color (R, G, B, A)

            // 6. Configure Core Dynamic Rendering Pass (`VkRenderingInfo`)
            VkRenderingInfo rendering_info{};
            rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO; // Core Vulkan 1.3 / 1.4 structure type
            rendering_info.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;

            // 7. Begin Core Dynamic Rendering Pass
            vkCmdBeginRenderingFunc(commandBuffer, &rendering_info);

            // 8. Bind Graphics Pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            // 9. Set Dynamic Viewport & Scissor
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(WIDTH);
            viewport.height = static_cast<float>(HEIGHT);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {WIDTH, HEIGHT};
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // 10. Record Draw Command (3 Vertices for Triangle)
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            // 11. End Core Dynamic Rendering Pass
            vkCmdEndRenderingFunc(commandBuffer);

            // 12. Vulkan 1.4 Native Synchronization2 Image Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
            VkImageMemoryBarrier2 barrierToPresent{};
            barrierToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2; // Core Vulkan 1.4 Synchronization2 Barrier
            barrierToPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToPresent.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrierToPresent.dstAccessMask = VK_ACCESS_2_NONE;
            barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrierToPresent.image = swapchainImages[imageIndex];
            barrierToPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrierToPresent.subresourceRange.baseMipLevel = 0;
            barrierToPresent.subresourceRange.levelCount = 1;
            barrierToPresent.subresourceRange.baseArrayLayer = 0;
            barrierToPresent.subresourceRange.layerCount = 1;

            VkDependencyInfo dependencyInfoPresent{};
            dependencyInfoPresent.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfoPresent.imageMemoryBarrierCount = 1;
            dependencyInfoPresent.pImageMemoryBarriers = &barrierToPresent;

            // Execute Vulkan 1.4 Synchronization2 Pipeline Barrier
            vkCmdPipelineBarrier2Func(commandBuffer, &dependencyInfoPresent);

            // 13. Finish Command Buffer Recording
            vk_common::check_vk_result(vkEndCommandBuffer(commandBuffer), "Failed to record command buffer");

            // 14. Submit Command Buffer to Queue
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            vk_common::check_vk_result(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence), "Failed to submit draw command buffer");

            // 15. Queue Swapchain Image for Presentation
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            VkSwapchainKHR swapchains[] = {swapchain};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapchains;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }

        vkDeviceWaitIdle(device);

        // STEP 17: Resource Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, fence, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);

        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "[Exception] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Vulkan 1.4 Application finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
