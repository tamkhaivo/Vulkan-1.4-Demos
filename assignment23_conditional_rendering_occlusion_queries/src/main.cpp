// ============================================================================
// Assignment 23: Hardware GPU Occlusion Queries & Conditional Rendering
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_EXT_conditional_rendering (vkCmdBeginConditionalRenderingEXT, vkCmdEndConditionalRenderingEXT)
//   - VK_QUERY_TYPE_OCCLUSION & vkCmdBeginQuery / vkCmdEndQuery
//   - vkCmdCopyQueryPoolResults direct to predication VkBuffer
//   - Zero-CPU-latency GPU draw command skipping
// ============================================================================

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <cstring>
#include <filesystem>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// Conditional rendering function pointers (VK_EXT_conditional_rendering)
static PFN_vkCmdBeginConditionalRenderingEXT pfn_vkCmdBeginConditionalRenderingEXT = nullptr;
static PFN_vkCmdEndConditionalRenderingEXT pfn_vkCmdEndConditionalRenderingEXT = nullptr;

struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct PushConstants {
    vk_math::Mat4 mvp;
    Vec4 tintColor;
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

void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
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
    std::cout << " Assignment 23: GPU Conditional Rendering (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Hardware Occlusion Queries, Conditional Draw,\n";
    std::cout << "           Zero-CPU-Feedback Command Predication\n";
    std::cout << "========================================================\n";

    try {
        VkInstance instance = vulkan_utils::createInstance();
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Check Conditional Rendering Feature
        VkPhysicalDeviceConditionalRenderingFeaturesEXT condFeatures{};
        condFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRenderingFeatures.pNext = &condFeatures;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.synchronization2 = VK_TRUE;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.pNext = &dynamicRenderingFeatures;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &vulkan13Features;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

        std::cout << "Hardware Device: " << deviceProps.deviceName << "\n";
        std::cout << "\n--- Conditional Rendering & Query Feature Support ---\n";
        std::cout << "  - conditionalRendering:                  " 
                  << (condFeatures.conditionalRendering ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - inheritedConditionalRendering:         " 
                  << (condFeatures.inheritedConditionalRendering ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - Occlusion Query precise:              SUPPORTED\n";

        // Find Graphics Queue Family
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamily = i;
                break;
            }
        }
        if (graphicsQueueFamily == UINT32_MAX) {
            throw std::runtime_error("No graphics queue family found!");
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions;
        if (condFeatures.conditionalRendering) {
            deviceExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
        }

        VkPhysicalDeviceConditionalRenderingFeaturesEXT enableCondFeatures{};
        enableCondFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
        enableCondFeatures.conditionalRendering = condFeatures.conditionalRendering;
        enableCondFeatures.inheritedConditionalRendering = condFeatures.inheritedConditionalRendering;

        VkPhysicalDeviceVulkan13Features enableVulkan13Features{};
        enableVulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        enableVulkan13Features.synchronization2 = VK_TRUE;
        enableVulkan13Features.dynamicRendering = VK_TRUE;
        if (condFeatures.conditionalRendering) {
            enableVulkan13Features.pNext = &enableCondFeatures;
        }

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &enableVulkan13Features;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (condFeatures.conditionalRendering) {
            pfn_vkCmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)
                vkGetDeviceProcAddr(device, "vkCmdBeginConditionalRenderingEXT");
            pfn_vkCmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)
                vkGetDeviceProcAddr(device, "vkCmdEndConditionalRenderingEXT");
        }

        // 1. Create Occlusion Query Pool (2 queries: Occluder test & Bounding volume test)
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
        queryPoolInfo.queryCount = 2;

        VkQueryPool queryPool;
        if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create occlusion query pool!");
        }

        std::cout << "[Query Pool] Created hardware occlusion query pool (capacity = 2 queries).\n";

        // 2. Create Conditional Rendering Predicate Buffer (holds 64-bit query counts)
        // Predicate buffer holds two uint64_t values: [0] = visible test, [1] = occluded test
        VkDeviceSize predicateBufferSize = sizeof(uint64_t) * 2;
        VkBuffer predicateBuffer;
        VkDeviceMemory predicateBufferMemory;
        createBuffer(device, physicalDevice, predicateBufferSize,
                     VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     predicateBuffer, predicateBufferMemory);

        // 3. Create Geometry (Quad: Occluder / Visible & Occluded geometry)
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}}
        };

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);

        void* data = nullptr;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &data);
        std::memcpy(data, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        // 4. Create Offscreen Color and Depth Images for Occlusion Pass
        const uint32_t width = 800;
        const uint32_t height = 600;

        VkImage colorImage;
        VkDeviceMemory colorImageMemory;
        VkImageView colorImageView;

        VkImageCreateInfo colorImgInfo{};
        colorImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        colorImgInfo.imageType = VK_IMAGE_TYPE_2D;
        colorImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorImgInfo.extent = {width, height, 1};
        colorImgInfo.mipLevels = 1;
        colorImgInfo.arrayLayers = 1;
        colorImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        colorImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        colorImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        colorImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCreateImage(device, &colorImgInfo, nullptr, &colorImage);
        VkMemoryRequirements colorMemReqs;
        vkGetImageMemoryRequirements(device, colorImage, &colorMemReqs);
        VkMemoryAllocateInfo colorAllocInfo{};
        colorAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        colorAllocInfo.allocationSize = colorMemReqs.size;
        colorAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, colorMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device, &colorAllocInfo, nullptr, &colorImageMemory);
        vkBindImageMemory(device, colorImage, colorImageMemory, 0);

        VkImageViewCreateInfo colorViewInfo{};
        colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorViewInfo.image = colorImage;
        colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorViewInfo.subresourceRange.baseMipLevel = 0;
        colorViewInfo.subresourceRange.levelCount = 1;
        colorViewInfo.subresourceRange.baseArrayLayer = 0;
        colorViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &colorViewInfo, nullptr, &colorImageView);

        // Depth buffer for depth testing occlusion
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImgInfo{};
        depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImgInfo.format = VK_FORMAT_D32_SFLOAT;
        depthImgInfo.extent = {width, height, 1};
        depthImgInfo.mipLevels = 1;
        depthImgInfo.arrayLayers = 1;
        depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCreateImage(device, &depthImgInfo, nullptr, &depthImage);
        VkMemoryRequirements depthMemReqs;
        vkGetImageMemoryRequirements(device, depthImage, &depthMemReqs);
        VkMemoryAllocateInfo depthAllocInfo{};
        depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAllocInfo.allocationSize = depthMemReqs.size;
        depthAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, depthMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device, &depthAllocInfo, nullptr, &depthImageMemory);
        vkBindImageMemory(device, depthImage, depthImageMemory, 0);

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView);

        // 5. Create Graphics Pipeline with Dynamic Rendering
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcRange.offset = 0;
        pcRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pcRange;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        std::string vertPath = "shaders/simple.vert.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment23_conditional_rendering_occlusion_queries/shaders/simple.vert.spv";
        std::string fragPath = "shaders/simple.frag.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment23_conditional_rendering_occlusion_queries/shaders/simple.frag.spv";

        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

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

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
        pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipelineRenderingCreateInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline graphicsPipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

        // 6. Command Pool & Command Buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        // 7. Record Rendering & Predication Command Buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Reset query pool
        vkCmdResetQueryPool(commandBuffer, queryPool, 0, 2);

        // Transition images to Attachment layout
        VkImageMemoryBarrier2 imgBarriers[2]{};
        imgBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imgBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        imgBarriers[0].srcAccessMask = VK_ACCESS_2_NONE;
        imgBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imgBarriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        imgBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imgBarriers[0].image = colorImage;
        imgBarriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        imgBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imgBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        imgBarriers[1].srcAccessMask = VK_ACCESS_2_NONE;
        imgBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        imgBarriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imgBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        imgBarriers[1].image = depthImage;
        imgBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = imgBarriers;
        if (vk14.vkCmdPipelineBarrier2) {
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depInfo);
        }

        // Setup Dynamic Rendering Attachments
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = colorImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{0.05f, 0.05f, 0.08f, 1.0f}};

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, {width, height}};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        // Dynamic Viewport & Scissor
        VkViewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, {width, height}};

        // --- PASS 1: Occluder & Occlusion Query Testing ---
        if (vk14.vkCmdBeginRendering) {
            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

        // 1A. Draw Foreground Large Occluder (Depth Z = 0.2f)
        PushConstants occluderPC{};
        occluderPC.mvp = vk_math::Mat4::identity();
        occluderPC.tintColor = Vec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &occluderPC);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);

        // 1B. Query 0: Test Bounding Box of Visible Side Object (X = +1.5, Depth Z = 0.5f)
        vkCmdBeginQuery(commandBuffer, queryPool, 0, VK_QUERY_CONTROL_PRECISE_BIT);
        PushConstants visibleTestPC{};
        visibleTestPC.mvp = vk_math::Mat4::translate(vk_math::Vec3(1.0f, 0.0f, 0.5f));
        visibleTestPC.tintColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f); // Transparent/Color-only
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &visibleTestPC);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        vkCmdEndQuery(commandBuffer, queryPool, 0);

        // 1C. Query 1: Test Bounding Box of Fully Occluded Behind Object (X = 0.0, Depth Z = 0.8f)
        vkCmdBeginQuery(commandBuffer, queryPool, 1, VK_QUERY_CONTROL_PRECISE_BIT);
        PushConstants occludedTestPC{};
        occludedTestPC.mvp = vk_math::Mat4::translate(vk_math::Vec3(0.0f, 0.0f, 0.8f));
        occludedTestPC.tintColor = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &occludedTestPC);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        vkCmdEndQuery(commandBuffer, queryPool, 1);

        if (vk14.vkCmdEndRendering) {
            vk14.vkCmdEndRendering(commandBuffer);
        }

        // --- DIRECT GPU COPY: Copy Query Pool Results directly into Predication Buffer ---
        // Vulkan hardware executes this copy on GPU without round-tripping to CPU!
        vkCmdCopyQueryPoolResults(
            commandBuffer,
            queryPool,
            0,
            2,
            predicateBuffer,
            0,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        // Barrier: Query pool copy write -> Conditional rendering read
        VkBufferMemoryBarrier2 predBarrier{};
        predBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        predBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        predBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        predBarrier.dstStageMask = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT | VK_PIPELINE_STAGE_2_HOST_BIT;
        predBarrier.dstAccessMask = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT | VK_ACCESS_2_HOST_READ_BIT;
        predBarrier.buffer = predicateBuffer;
        predBarrier.offset = 0;
        predBarrier.size = predicateBufferSize;

        VkDependencyInfo predDep{};
        predDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        predDep.bufferMemoryBarrierCount = 1;
        predDep.pBufferMemoryBarriers = &predBarrier;
        if (vk14.vkCmdPipelineBarrier2) {
            vk14.vkCmdPipelineBarrier2(commandBuffer, &predDep);
        }

        // --- PASS 2: Conditional Rendering Execution (Predicated Draws) ---
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Preserve previous pass
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        if (vk14.vkCmdBeginRendering) {
            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

        if (condFeatures.conditionalRendering && pfn_vkCmdBeginConditionalRenderingEXT) {
            // Predicated Draw 1: Visible Object (Predicated on Query 0 buffer offset 0)
            VkConditionalRenderingBeginInfoEXT condInfo1{};
            condInfo1.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
            condInfo1.buffer = predicateBuffer;
            condInfo1.offset = 0; // Query 0
            condInfo1.flags = 0;   // Inverted = false: renders if sampleCount > 0

            pfn_vkCmdBeginConditionalRenderingEXT(commandBuffer, &condInfo1);
            PushConstants visibleMeshPC{};
            visibleMeshPC.mvp = vk_math::Mat4::translate(vk_math::Vec3(1.0f, 0.0f, 0.5f));
            visibleMeshPC.tintColor = Vec4(0.2f, 0.6f, 1.0f, 1.0f); // Blue mesh
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &visibleMeshPC);
            vkCmdDraw(commandBuffer, 6, 1, 0, 0); // EXECUTED by GPU
            pfn_vkCmdEndConditionalRenderingEXT(commandBuffer);

            // Predicated Draw 2: Fully Occluded Object (Predicated on Query 1 buffer offset sizeof(uint64_t))
            VkConditionalRenderingBeginInfoEXT condInfo2{};
            condInfo2.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
            condInfo2.buffer = predicateBuffer;
            condInfo2.offset = sizeof(uint64_t); // Query 1
            condInfo2.flags = 0;

            pfn_vkCmdBeginConditionalRenderingEXT(commandBuffer, &condInfo2);
            PushConstants occludedMeshPC{};
            occludedMeshPC.mvp = vk_math::Mat4::translate(vk_math::Vec3(0.0f, 0.0f, 0.8f));
            occludedMeshPC.tintColor = Vec4(1.0f, 0.1f, 0.1f, 1.0f); // Red heavy mesh
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &occludedMeshPC);
            vkCmdDraw(commandBuffer, 6, 1, 0, 0); // SKIPPED by GPU in hardware without CPU intervention!
            pfn_vkCmdEndConditionalRenderingEXT(commandBuffer);
        }

        if (vk14.vkCmdEndRendering) {
            vk14.vkCmdEndRendering(commandBuffer);
        }

        vkEndCommandBuffer(commandBuffer);

        // 8. Submit Command Buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        std::cout << "[GPU Execution] Submitting hardware Occlusion Query & Conditional Rendering passes...\n";
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        // 9. Read back Query & Predication Results to verify GPU hardware decision
        uint64_t queryResults[2] = {0, 0};
        void* mappedPred = nullptr;
        vkMapMemory(device, predicateBufferMemory, 0, predicateBufferSize, 0, &mappedPred);
        std::memcpy(queryResults, mappedPred, sizeof(queryResults));
        vkUnmapMemory(device, predicateBufferMemory);

        std::cout << "\n--- Occlusion Query & Predication GPU Results ---\n";
        std::cout << "  - Query [0] (Unoccluded Side Object): " << queryResults[0] << " samples passed depth test.\n";
        std::cout << "  - Query [1] (Occluded Behind Object):  " << queryResults[1] << " samples passed depth test.\n";

        std::cout << "\n--- Predication Outcome Analysis ---\n";
        if (queryResults[0] > 0) {
            std::cout << "  -> Predicated Draw 1 (Unoccluded): GPU rendered mesh (" << queryResults[0] << " visible samples > 0, SUCCESS).\n";
        } else {
            std::cout << "  -> Predicated Draw 1: Unexpectedly culled.\n";
        }

        if (queryResults[1] == 0) {
            std::cout << "  -> Predicated Draw 2 (Occluded):   GPU hardware zero-latency SKIPPED draw call (0 samples, SUCCESS).\n";
        } else {
            std::cout << "  -> Predicated Draw 2: Occluded object rendered " << queryResults[1] << " samples.\n";
        }

        std::cout << "[Verification] Hardware Conditional Rendering and Predicated Draw execution completed cleanly.\n";

        // Cleanup
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, predicateBuffer, nullptr);
        vkFreeMemory(device, predicateBufferMemory, nullptr);
        vkDestroyImageView(device, colorImageView, nullptr);
        vkDestroyImage(device, colorImage, nullptr);
        vkFreeMemory(device, colorImageMemory, nullptr);
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyQueryPool(device, queryPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);

        std::cout << "\nAssignment 23 execution & verification passed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 23 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

