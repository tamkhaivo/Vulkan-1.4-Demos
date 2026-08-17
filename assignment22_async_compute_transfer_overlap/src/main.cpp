// ============================================================================
// Assignment 22: Asynchronous Multi-Queue Concurrency & Transfer Overlap
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Dedicated Async Compute Queue & Graphics Queue execution
//   - Cross-queue Timeline Semaphore synchronization
//   - GPU particle physics simulation in compute queue overlapped with rendering
//   - Dynamic Rendering with additive alpha blending
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <stdexcept>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct ComputePushConstants {
    float dt;
    uint32_t particleCount;
};

struct GraphicsPushConstants {
    vk_math::Mat4 mvp;
};

const uint32_t NUM_PARTICLES = 32768;

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

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t graphicsFamily = 0, uint32_t computeFamily = 0) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    uint32_t families[2] = { graphicsFamily, computeFamily };
    if (graphicsFamily != computeFamily) {
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bufferInfo.queueFamilyIndexCount = 2;
        bufferInfo.pQueueFamilyIndices = families;
    } else {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
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
    std::cout << " Assignment 22: Async Compute Multi-Queue Overlap (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Dedicated Async Compute Queue, Timeline Semaphores,\n";
    std::cout << "           SSBO Particle Simulation, Non-blocking Multi-Engine Overlap\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 22: Async Compute Multi-Queue Overlap (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Discover Queue Families (Graphics and Compute)
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t computeFamily = UINT32_MAX;

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport && graphicsFamily == UINT32_MAX) {
                graphicsFamily = i;
            }
            if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && (i != graphicsFamily || computeFamily == UINT32_MAX)) {
                computeFamily = i;
            }
        }

        std::cout << "Graphics Queue Family: " << graphicsFamily << "\n";
        std::cout << "Compute Queue Family:  " << computeFamily << "\n";

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float priority = 1.0f;

        VkDeviceQueueCreateInfo graphicsQueueInfo{};
        graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicsQueueInfo.queueFamilyIndex = graphicsFamily;
        graphicsQueueInfo.queueCount = 1;
        graphicsQueueInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(graphicsQueueInfo);

        if (computeFamily != graphicsFamily) {
            VkDeviceQueueCreateInfo computeQueueInfo{};
            computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            computeQueueInfo.queueFamilyIndex = computeFamily;
            computeQueueInfo.queueCount = 1;
            computeQueueInfo.pQueuePriorities = &priority;
            queueCreateInfos.push_back(computeQueueInfo);
        }

        // Enable timeline semaphores, dynamic rendering & shaderPointSize
        VkPhysicalDeviceVulkan12Features vulkan12Features{};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.timelineSemaphore = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &vulkan12Features;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;
        deviceFeatures2.features.largePoints = VK_TRUE;

        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device");

        VkQueue graphicsQueue, computeQueue;
        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // Create Timeline Semaphore for multi-queue synchronization
        VkSemaphoreTypeCreateInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineInfo.initialValue = 0;

        VkSemaphoreCreateInfo semCreateInfo{};
        semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semCreateInfo.pNext = &timelineInfo;

        VkSemaphore timelineSemaphore;
        vk_common::check_vk_result(vkCreateSemaphore(device, &semCreateInfo, nullptr, &timelineSemaphore), "Failed to create timeline semaphore");

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

        // Initialize Particle Data
        std::vector<Vec4> initialPositions(NUM_PARTICLES);
        std::vector<Vec4> initialVelocities(NUM_PARTICLES);

        std::default_random_engine rnd(42);
        std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * vk_math::PI);
        std::uniform_real_distribution<float> distSpeed(2.0f, 6.0f);
        std::uniform_real_distribution<float> distY(0.0f, 4.0f);

        for (uint32_t i = 0; i < NUM_PARTICLES; ++i) {
            float angle = distAngle(rnd);
            float speed = distSpeed(rnd);
            float r = ((float)rand() / (float)RAND_MAX) * 0.5f;

            initialPositions[i] = Vec4(std::cos(angle) * r, distY(rnd), std::sin(angle) * r, 1.0f);
            initialVelocities[i] = Vec4(std::cos(angle) * speed * 0.3f, speed, std::sin(angle) * speed * 0.3f, 0.0f);
        }

        VkDeviceSize bufferSize = sizeof(Vec4) * NUM_PARTICLES;

        VkBuffer posBuffer, velBuffer;
        VkDeviceMemory posMemory, velMemory;
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     posBuffer, posMemory, graphicsFamily, computeFamily);
        createBuffer(device, physicalDevice, bufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     velBuffer, velMemory, graphicsFamily, computeFamily);

        void* mappedData;
        vkMapMemory(device, posMemory, 0, bufferSize, 0, &mappedData);
        std::memcpy(mappedData, initialPositions.data(), (size_t)bufferSize);
        vkUnmapMemory(device, posMemory);

        vkMapMemory(device, velMemory, 0, bufferSize, 0, &mappedData);
        std::memcpy(mappedData, initialVelocities.data(), (size_t)bufferSize);
        vkUnmapMemory(device, velMemory);

        // Compute Pipeline Setup
        VkDescriptorSetLayoutBinding computeBindings[2]{};
        computeBindings[0].binding = 0;
        computeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeBindings[0].descriptorCount = 1;
        computeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        computeBindings[1].binding = 1;
        computeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeBindings[1].descriptorCount = 1;
        computeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
        computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        computeLayoutInfo.bindingCount = 2;
        computeLayoutInfo.pBindings = computeBindings;

        VkDescriptorSetLayout computeDescriptorLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &computeLayoutInfo, nullptr, &computeDescriptorLayout), "Failed to create compute layout");

        VkPushConstantRange computePushRange{};
        computePushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        computePushRange.offset = 0;
        computePushRange.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
        computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computePipelineLayoutInfo.setLayoutCount = 1;
        computePipelineLayoutInfo.pSetLayouts = &computeDescriptorLayout;
        computePipelineLayoutInfo.pushConstantRangeCount = 1;
        computePipelineLayoutInfo.pPushConstantRanges = &computePushRange;

        VkPipelineLayout computePipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &computePipelineLayoutInfo, nullptr, &computePipelineLayout), "Failed to create compute pipeline layout");

        auto computeCode = vulkan_utils::readFile("shaders/simulation.comp.spv");
        VkShaderModule computeShaderModule = vulkan_utils::createShaderModule(device, computeCode);

        VkComputePipelineCreateInfo computeCreateInfo{};
        computeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computeCreateInfo.layout = computePipelineLayout;
        computeCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeCreateInfo.stage.module = computeShaderModule;
        computeCreateInfo.stage.pName = "main";

        VkPipeline computePipeline;
        vk_common::check_vk_result(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &computePipeline), "Failed to create compute pipeline");

        // Compute Descriptor Pool & Set
        VkDescriptorPoolSize compPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
        VkDescriptorPoolCreateInfo compPoolInfo{};
        compPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        compPoolInfo.maxSets = 1;
        compPoolInfo.poolSizeCount = 1;
        compPoolInfo.pPoolSizes = &compPoolSize;

        VkDescriptorPool computeDescriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &compPoolInfo, nullptr, &computeDescriptorPool), "Failed to create compute desc pool");

        VkDescriptorSetAllocateInfo compSetAlloc{};
        compSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        compSetAlloc.descriptorPool = computeDescriptorPool;
        compSetAlloc.descriptorSetCount = 1;
        compSetAlloc.pSetLayouts = &computeDescriptorLayout;

        VkDescriptorSet computeDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &compSetAlloc, &computeDescriptorSet), "Failed to allocate compute desc set");

        VkDescriptorBufferInfo bufferInfos[2]{};
        bufferInfos[0].buffer = posBuffer;
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = bufferSize;

        bufferInfos[1].buffer = velBuffer;
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = bufferSize;

        VkWriteDescriptorSet compWrites[2]{};
        compWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[0].dstSet = computeDescriptorSet;
        compWrites[0].dstBinding = 0;
        compWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compWrites[0].descriptorCount = 1;
        compWrites[0].pBufferInfo = &bufferInfos[0];

        compWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[1].dstSet = computeDescriptorSet;
        compWrites[1].dstBinding = 1;
        compWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compWrites[1].descriptorCount = 1;
        compWrites[1].pBufferInfo = &bufferInfos[1];

        vkUpdateDescriptorSets(device, 2, compWrites, 0, nullptr);

        // Graphics Pipeline Setup
        VkPushConstantRange graphicsPushRange{};
        graphicsPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        graphicsPushRange.offset = 0;
        graphicsPushRange.size = sizeof(GraphicsPushConstants);

        VkPipelineLayoutCreateInfo graphicsPipelineLayoutInfo{};
        graphicsPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        graphicsPipelineLayoutInfo.pushConstantRangeCount = 1;
        graphicsPipelineLayoutInfo.pPushConstantRanges = &graphicsPushRange;

        VkPipelineLayout graphicsPipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &graphicsPipelineLayoutInfo, nullptr, &graphicsPipelineLayout), "Failed to create graphics pipeline layout");

        auto vertCode = vulkan_utils::readFile("shaders/particle.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/particle.frag.spv");

        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragModule;
        shaderStages[1].pName = "main";

        VkVertexInputBindingDescription particleBindings[2]{};
        particleBindings[0].binding = 0;
        particleBindings[0].stride = sizeof(Vec4);
        particleBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        particleBindings[1].binding = 1;
        particleBindings[1].stride = sizeof(Vec4);
        particleBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription particleAttrs[2]{};
        particleAttrs[0].binding = 0;
        particleAttrs[0].location = 0;
        particleAttrs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        particleAttrs[0].offset = 0;

        particleAttrs[1].binding = 1;
        particleAttrs[1].location = 1;
        particleAttrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        particleAttrs[1].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 2;
        vertexInputInfo.pVertexBindingDescriptions = particleBindings;
        vertexInputInfo.vertexAttributeDescriptionCount = 2;
        vertexInputInfo.pVertexAttributeDescriptions = particleAttrs;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Additive alpha blending for glowing particle field
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

        VkGraphicsPipelineCreateInfo graphicsCreateInfo{};
        graphicsCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsCreateInfo.pNext = &renderingCreateInfo;
        graphicsCreateInfo.stageCount = 2;
        graphicsCreateInfo.pStages = shaderStages;
        graphicsCreateInfo.pVertexInputState = &vertexInputInfo;
        graphicsCreateInfo.pInputAssemblyState = &inputAssembly;
        graphicsCreateInfo.pViewportState = &viewportState;
        graphicsCreateInfo.pRasterizationState = &rasterizer;
        graphicsCreateInfo.pMultisampleState = &multisampling;
        graphicsCreateInfo.pColorBlendState = &colorBlending;
        graphicsCreateInfo.pDynamicState = &dynamicState;
        graphicsCreateInfo.layout = graphicsPipelineLayout;
        graphicsCreateInfo.renderPass = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsCreateInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // Command Pools for Compute & Graphics Queues
        VkCommandPoolCreateInfo graphicsPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        graphicsPoolInfo.queueFamilyIndex = graphicsFamily;
        VkCommandPool graphicsCommandPool;
        vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &graphicsCommandPool);

        VkCommandPoolCreateInfo computePoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        computePoolInfo.queueFamilyIndex = computeFamily;
        VkCommandPool computeCommandPool;
        vkCreateCommandPool(device, &computePoolInfo, nullptr, &computeCommandPool);

        VkCommandBufferAllocateInfo gAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, graphicsCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkCommandBuffer graphicsCmd;
        vkAllocateCommandBuffers(device, &gAlloc, &graphicsCmd);

        VkCommandBufferAllocateInfo cAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, computeCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkCommandBuffer computeCmd;
        vkAllocateCommandBuffers(device, &cAlloc, &computeCmd);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo scInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fcInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateSemaphore(device, &scInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &scInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fcInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Simulating & rendering " << NUM_PARTICLES << " particles via Async Compute + Graphics Overlap...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t timelineValue = 0;
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment22_async_compute_transfer_overlap
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment22_async_compute_transfer_overlap");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment22_async_compute_transfer_overlap");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                break;
            }

            timelineValue++;

            // 1. RECORD & SUBMIT ASYNC COMPUTE COMMAND BUFFER
            vkResetCommandBuffer(computeCmd, 0);
            VkCommandBufferBeginInfo compBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(computeCmd, &compBegin);

            vkCmdBindPipeline(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

            ComputePushConstants cpc{ 0.016f, NUM_PARTICLES };
            vkCmdPushConstants(computeCmd, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &cpc);
            vkCmdDispatch(computeCmd, (NUM_PARTICLES + 63) / 64, 1, 1);

            vkEndCommandBuffer(computeCmd);

            // Submit compute queue and signal timeline semaphore
            VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
            timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineSubmitInfo.signalSemaphoreValueCount = 1;
            timelineSubmitInfo.pSignalSemaphoreValues = &timelineValue;

            VkSubmitInfo computeSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            computeSubmit.pNext = &timelineSubmitInfo;
            computeSubmit.commandBufferCount = 1;
            computeSubmit.pCommandBuffers = &computeCmd;
            computeSubmit.signalSemaphoreCount = 1;
            computeSubmit.pSignalSemaphores = &timelineSemaphore;

            vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

            // 2. RECORD GRAPHICS COMMAND BUFFER
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            vkResetCommandBuffer(graphicsCmd, 0);
            VkCommandBufferBeginInfo gfxBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(graphicsCmd, &gfxBegin);

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

            VkDependencyInfo depToColor{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToColor.imageMemoryBarrierCount = 1;
            depToColor.pImageMemoryBarriers = &barrierToColor;
            vk14.vkCmdPipelineBarrier2(graphicsCmd, &depToColor);

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = { { { 0.02f, 0.02f, 0.04f, 1.0f } } };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vk14.vkCmdBeginRendering(graphicsCmd, &renderingInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(graphicsCmd, 0, 1, &viewport);
            vkCmdSetScissor(graphicsCmd, 0, 1, &scissor);

            vkCmdBindPipeline(graphicsCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkBuffer particleBuffers[2] = { posBuffer, velBuffer };
            VkDeviceSize offsets[2] = { 0, 0 };
            vkCmdBindVertexBuffers(graphicsCmd, 0, 2, particleBuffers, offsets);

            // Orbiting camera around particle fountain
            vk_math::Mat4 model = vk_math::Mat4::identity();
            float camX = std::cos(time * 0.4f) * 12.0f;
            float camZ = std::sin(time * 0.4f) * 12.0f;
            vk_math::Mat4 view = vk_math::Mat4::lookAt(vk_math::Vec3(camX, 4.0f, camZ), vk_math::Vec3(0.0f, 2.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

            GraphicsPushConstants gpc{};
            gpc.mvp = proj * view * model;
            vkCmdPushConstants(graphicsCmd, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GraphicsPushConstants), &gpc);

            vkCmdDraw(graphicsCmd, NUM_PARTICLES, 1, 0, 0);

            vk14.vkCmdEndRendering(graphicsCmd);

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
            vk14.vkCmdPipelineBarrier2(graphicsCmd, &depToPresent);

            vkEndCommandBuffer(graphicsCmd);

            // 3. SUBMIT GRAPHICS QUEUE (WAITS ON TIMELINE SEMAPHORE FROM COMPUTE)
            uint64_t waitTimelineValues[2] = { 0, timelineValue };
            VkTimelineSemaphoreSubmitInfo graphicsTimelineInfo{};
            graphicsTimelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            graphicsTimelineInfo.waitSemaphoreValueCount = 2;
            graphicsTimelineInfo.pWaitSemaphoreValues = waitTimelineValues;

            VkSemaphore waitSemaphores[2] = { imageAvailableSemaphore, timelineSemaphore };
            VkPipelineStageFlags waitStages[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };

            VkSubmitInfo graphicsSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            graphicsSubmit.pNext = &graphicsTimelineInfo;
            graphicsSubmit.waitSemaphoreCount = 2;
            graphicsSubmit.pWaitSemaphores = waitSemaphores;
            graphicsSubmit.pWaitDstStageMask = waitStages;
            graphicsSubmit.commandBufferCount = 1;
            graphicsSubmit.pCommandBuffers = &graphicsCmd;
            graphicsSubmit.signalSemaphoreCount = 1;
            graphicsSubmit.pSignalSemaphores = &renderFinishedSemaphore;

            vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, inFlightFence);

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
        profiler.exportFoldedFile("flamegraph_assignment22_async_compute_transfer_overlap.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment22_async_compute_transfer_overlap.html");
        profiler.exportChromeTraceFile("flamegraph_assignment22_async_compute_transfer_overlap.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, timelineSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, posBuffer, nullptr);
        vkFreeMemory(device, posMemory, nullptr);
        vkDestroyBuffer(device, velBuffer, nullptr);
        vkFreeMemory(device, velMemory, nullptr);

        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyShaderModule(device, computeShaderModule, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

        vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
        vkDestroyCommandPool(device, computeCommandPool, nullptr);

        for (auto view : swapchainImageViews) {
            vkDestroyImageView(device, view, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "\nAssignment 22 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 22 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
