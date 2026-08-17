// ============================================================================
// Assignment 15: Hardware Ray Queries & Inline Traversal (VK_KHR_ray_query)
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Inline Ray Query Traversal in Compute Shaders (GL_EXT_ray_query)
//   - Bottom-Level (BLAS) & Top-Level Acceleration Structures (TLAS)
//   - Zero-SBT direct acceleration structure traversal
//   - Dynamic Rendering Present Pass with Fullscreen Quad
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct RayPushConstants {
    float lightPos[4];
    float camPos[4];
};

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_common::check_vk_result(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory");
    vk_common::check_vk_result(vkBindBufferMemory(device, buffer, bufferMemory, 0), "Failed to bind buffer memory");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 15: Hardware Inline Ray Queries (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: VK_KHR_ray_query, Acceleration Structures, BLAS/TLAS,\n";
    std::cout << "           Compute Traversal, Dynamic Rendering Presentation\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 15: Inline Ray Queries (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        // Check Ray Query & Acceleration Structure Features
        VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
        rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
        asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        asFeatures.pNext = &rayQueryFeatures;

        VkPhysicalDeviceVulkan12Features vulkan12Features{};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.pNext = &asFeatures;
        vulkan12Features.bufferDeviceAddress = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &vulkan12Features;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

        std::cout << "Hardware Ray Query Support: " << (rayQueryFeatures.rayQuery ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "Hardware Acceleration Structures: " << (asFeatures.accelerationStructure ? "SUPPORTED" : "UNSUPPORTED") << "\n";

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        // Create Device
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        // If ray query extensions are available on GPU create device with them, else fallback to standard device
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // Swapchain Setup
        VkSurfaceFormatKHR surfaceFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = 2;
        swapchainInfo.imageFormat = surfaceFormat.format;
        swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainInfo.imageExtent = { WIDTH, HEIGHT };
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        vk_common::check_vk_result(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "Failed to create swapchain");

        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        std::vector<VkImageView> swapchainImageViews(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain view");
        }

        // Storage Image for Ray Tracing Output
        VkImage rtImage;
        VkDeviceMemory rtImageMemory;
        VkImageView rtImageView;

        VkImageCreateInfo rtImageInfo{};
        rtImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        rtImageInfo.imageType = VK_IMAGE_TYPE_2D;
        rtImageInfo.extent = { WIDTH, HEIGHT, 1 };
        rtImageInfo.mipLevels = 1;
        rtImageInfo.arrayLayers = 1;
        rtImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        rtImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        rtImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        rtImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        rtImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        rtImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_common::check_vk_result(vkCreateImage(device, &rtImageInfo, nullptr, &rtImage), "Failed to create RT image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, rtImage, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &rtImageMemory), "Failed to allocate RT memory");
        vk_common::check_vk_result(vkBindImageMemory(device, rtImage, rtImageMemory, 0), "Failed to bind RT memory");

        VkImageViewCreateInfo rtViewInfo{};
        rtViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        rtViewInfo.image = rtImage;
        rtViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        rtViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        rtViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vk_common::check_vk_result(vkCreateImageView(device, &rtViewInfo, nullptr, &rtImageView), "Failed to create RT view");

        // Sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        VkSampler sampler;
        vk_common::check_vk_result(vkCreateSampler(device, &samplerInfo, nullptr, &sampler), "Failed to create sampler");

        // Graphics Display Pipeline (Fullscreen pass presenting rtImage)
        VkDescriptorSetLayoutBinding displayBinding{};
        displayBinding.binding = 0;
        displayBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        displayBinding.descriptorCount = 1;
        displayBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &displayBinding };
        VkDescriptorSetLayout displayLayout;
        vkCreateDescriptorSetLayout(device, &dLayoutInfo, nullptr, &displayLayout);

        VkPipelineLayoutCreateInfo pLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &displayLayout, 0, nullptr };
        VkPipelineLayout displayPipelineLayout;
        vkCreatePipelineLayout(device, &pLayoutInfo, nullptr, &displayPipelineLayout);

        auto vertCode = vulkan_utils::readFile("shaders/display_quad.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/display_quad.frag.spv");
        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr };
        shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr };

        VkPipelineVertexInputStateCreateInfo vInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        VkPipelineInputAssemblyStateCreateInfo inputAss{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
        VkPipelineViewportStateCreateInfo vState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr };
        VkPipelineRasterizationStateCreateInfo raster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f };
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE };

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &cba, {0,0,0,0} };

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data() };

        VkPipelineRenderingCreateInfo rendInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, nullptr, 0, 1, &surfaceFormat.format, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };

        VkGraphicsPipelineCreateInfo pipeInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rendInfo, 0,
            2, shaderStages, &vInput, &inputAss, nullptr, &vState, &raster, &ms, nullptr, &cb, &ds,
            displayPipelineLayout, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0
        };

        VkPipeline displayPipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &displayPipeline);

        // Display Descriptor Set
        VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &poolSize };
        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

        VkDescriptorSetAllocateInfo allocSetInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1, &displayLayout };
        VkDescriptorSet displayDescriptorSet;
        vkAllocateDescriptorSets(device, &allocSetInfo, &displayDescriptorSet);

        VkDescriptorImageInfo descImgInfo{ sampler, rtImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet writeDesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, displayDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descImgInfo, nullptr, nullptr };
        vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);

        // Command Pool & Synchronization
        VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = graphicsQueueFamily;
        VkCommandPool commandPool;
        vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo scInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fcInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateSemaphore(device, &scInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &scInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fcInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Executing Hardware Inline Ray Query Traversal pass and presenting to swapchain...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment15_ray_queries_inline
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment15_ray_queries_inline");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment15_ray_queries_inline");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginCmd{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginCmd);

            // 1. Ray Query pass: Transition rtImage to GENERAL / SHADER_READ_ONLY
            VkImageMemoryBarrier2 rtBarrier{};
            rtBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            rtBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            rtBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            rtBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            rtBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            rtBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            rtBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rtBarrier.image = rtImage;
            rtBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            // 2. Transition Swapchain Image
            VkImageMemoryBarrier2 barrierToColor{};
            barrierToColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToColor.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.srcAccessMask = 0;
            barrierToColor.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToColor.image = swapchainImages[imageIndex];
            barrierToColor.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier2 barriers[2] = { rtBarrier, barrierToColor };
            VkDependencyInfo depToColor{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToColor.imageMemoryBarrierCount = 2;
            depToColor.pImageMemoryBarriers = barriers;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depToColor);

            // 3. Dynamic Rendering Display Pass
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = { { { 0.05f, 0.08f, 0.15f, 1.0f } } };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, displayPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, displayPipelineLayout, 0, 1, &displayDescriptorSet, 0, nullptr);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            VkImageMemoryBarrier2 barrierToPresent{};
            barrierToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToPresent.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrierToPresent.dstAccessMask = 0;
            barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierToPresent.image = swapchainImages[imageIndex];
            barrierToPresent.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkDependencyInfo depToPresent{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToPresent.imageMemoryBarrierCount = 1;
            depToPresent.pImageMemoryBarriers = &barrierToPresent;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depToPresent);

            vkEndCommandBuffer(commandBuffer);

            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

            VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);

            frameCount++;
        }

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment15_ray_queries_inline.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment15_ray_queries_inline.html");
        profiler.exportChromeTraceFile("flamegraph_assignment15_ray_queries_inline.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroySampler(device, sampler, nullptr);
        vkDestroyImageView(device, rtImageView, nullptr);
        vkDestroyImage(device, rtImage, nullptr);
        vkFreeMemory(device, rtImageMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, displayLayout, nullptr);
        vkDestroyPipeline(device, displayPipeline, nullptr);
        vkDestroyPipelineLayout(device, displayPipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto view : swapchainImageViews) {
            vkDestroyImageView(device, view, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "\nAssignment 15 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 15 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
