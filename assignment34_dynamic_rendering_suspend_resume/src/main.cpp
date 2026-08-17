// ============================================================================
// Assignment 34: Dynamic Rendering Suspend/Resume & Feedback Loops
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_RENDERING_SUSPENDING_BIT & VK_RENDERING_RESUMING_BIT
//   - Multi-pass execution without attachment re-clearing or intermediate texture allocations
//   - Sequential command buffer continuation for multi-stage rendering pipelines
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

// Background Floor Grid Geometry (Drawn during Suspend Pass 1)
void generateFloorGeometry(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    const float S = 2.0f;
    const float Y = -0.8f;
    vertices.push_back({ {-S, Y, -S}, {0.1f, 0.2f, 0.4f} });
    vertices.push_back({ { S, Y, -S}, {0.2f, 0.4f, 0.7f} });
    vertices.push_back({ { S, Y,  S}, {0.4f, 0.2f, 0.6f} });
    vertices.push_back({ {-S, Y,  S}, {0.2f, 0.1f, 0.3f} });

    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(2); indices.push_back(3); indices.push_back(0);
}

// Foreground Crystal Geometry (Drawn during Resume Pass 2)
void generateCrystalGeometry(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    const float R = 0.5f;
    const float H = 0.75f;

    vertices.push_back({ {0.0f,  H, 0.0f}, {1.0f, 0.9f, 0.3f} }); // Top (0)
    vertices.push_back({ {0.0f, -H, 0.0f}, {0.1f, 0.9f, 1.0f} }); // Bottom (1)

    for (int i = 0; i < 6; ++i) {
        float angle = float(i) * 2.0f * vk_math::PI / 6.0f;
        float x = std::cos(angle) * R;
        float z = std::sin(angle) * R;
        vk_math::Vec3 col = (i % 2 == 0) ? vk_math::Vec3(1.0f, 0.3f, 0.7f) : vk_math::Vec3(0.3f, 1.0f, 0.8f);
        vertices.push_back({ {x, 0.0f, z}, col });
    }

    for (uint16_t i = 0; i < 6; ++i) {
        uint16_t curr = 2 + i;
        uint16_t next = 2 + ((i + 1) % 6);
        indices.push_back(0); indices.push_back(curr); indices.push_back(next);
        indices.push_back(1); indices.push_back(next); indices.push_back(curr);
    }
}

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
    std::cout << " Assignment 34: Dynamic Rendering Suspend & Resume (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: VK_RENDERING_SUSPENDING_BIT, VK_RENDERING_RESUMING_BIT,\n";
    std::cout << "           Multi-Pass Continuation without Attachment Re-clearing\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 34: Dynamic Rendering Suspend/Resume (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
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

        // Depth Attachment Setup
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent = { WIDTH, HEIGHT, 1 };
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.format = depthFormat;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_common::check_vk_result(vkCreateImage(device, &depthImageInfo, nullptr, &depthImage), "Failed to create depth image");

        VkMemoryRequirements depthReqs;
        vkGetImageMemoryRequirements(device, depthImage, &depthReqs);
        VkMemoryAllocateInfo depthAlloc{};
        depthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAlloc.allocationSize = depthReqs.size;
        depthAlloc.memoryTypeIndex = findMemoryType(physicalDevice, depthReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vk_common::check_vk_result(vkAllocateMemory(device, &depthAlloc, nullptr, &depthImageMemory), "Failed to allocate depth memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth memory");

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        vk_common::check_vk_result(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView), "Failed to create depth view");

        // Pipeline Layout
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // Shaders
        auto vertCode = vulkan_utils::readFile("shaders/feedback_base.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/feedback_proc.frag.spv");

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

        auto bindingDesc = Vertex::getBindingDescription();
        auto attrDescs = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

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
        renderingCreateInfo.depthAttachmentFormat = depthFormat;

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
        graphicsCreateInfo.pDepthStencilState = &depthStencil;
        graphicsCreateInfo.pColorBlendState = &colorBlending;
        graphicsCreateInfo.pDynamicState = &dynamicState;
        graphicsCreateInfo.layout = pipelineLayout;
        graphicsCreateInfo.renderPass = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsCreateInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // Pass 1 Geometry (Floor)
        std::vector<Vertex> floorVertices;
        std::vector<uint16_t> floorIndices;
        generateFloorGeometry(floorVertices, floorIndices);

        VkDeviceSize floorVSize = sizeof(floorVertices[0]) * floorVertices.size();
        VkBuffer floorVBuffer;
        VkDeviceMemory floorVMem;
        createBuffer(device, physicalDevice, floorVSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, floorVBuffer, floorVMem);
        void* fvData;
        vkMapMemory(device, floorVMem, 0, floorVSize, 0, &fvData);
        std::memcpy(fvData, floorVertices.data(), (size_t)floorVSize);
        vkUnmapMemory(device, floorVMem);

        VkDeviceSize floorISize = sizeof(floorIndices[0]) * floorIndices.size();
        VkBuffer floorIBuffer;
        VkDeviceMemory floorIMem;
        createBuffer(device, physicalDevice, floorISize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, floorIBuffer, floorIMem);
        void* fiData;
        vkMapMemory(device, floorIMem, 0, floorISize, 0, &fiData);
        std::memcpy(fiData, floorIndices.data(), (size_t)floorISize);
        vkUnmapMemory(device, floorIMem);

        // Pass 2 Geometry (Crystal)
        std::vector<Vertex> crystalVertices;
        std::vector<uint16_t> crystalIndices;
        generateCrystalGeometry(crystalVertices, crystalIndices);

        VkDeviceSize crystalVSize = sizeof(crystalVertices[0]) * crystalVertices.size();
        VkBuffer crystalVBuffer;
        VkDeviceMemory crystalVMem;
        createBuffer(device, physicalDevice, crystalVSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, crystalVBuffer, crystalVMem);
        void* cvData;
        vkMapMemory(device, crystalVMem, 0, crystalVSize, 0, &cvData);
        std::memcpy(cvData, crystalVertices.data(), (size_t)crystalVSize);
        vkUnmapMemory(device, crystalVMem);

        VkDeviceSize crystalISize = sizeof(crystalIndices[0]) * crystalIndices.size();
        VkBuffer crystalIBuffer;
        VkDeviceMemory crystalIMem;
        createBuffer(device, physicalDevice, crystalISize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, crystalIBuffer, crystalIMem);
        void* ciData;
        vkMapMemory(device, crystalIMem, 0, crystalISize, 0, &ciData);
        std::memcpy(ciData, crystalIndices.data(), (size_t)crystalISize);
        vkUnmapMemory(device, crystalIMem);

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

        std::cout << "[Render Loop] Executing Multi-Pass Suspend (Part 1) and Resume (Part 2) Dynamic Rendering...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment34_dynamic_rendering_suspend_resume
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment34_dynamic_rendering_suspend_resume");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment34_dynamic_rendering_suspend_resume");
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

            VkImageMemoryBarrier2 barrierToDepth{};
            barrierToDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToDepth.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            barrierToDepth.srcAccessMask = 0;
            barrierToDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            barrierToDepth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrierToDepth.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            barrierToDepth.image = depthImage;
            barrierToDepth.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier2 barriers[2] = { barrierToColor, barrierToDepth };
            VkDependencyInfo depToColor{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToColor.imageMemoryBarrierCount = 2;
            depToColor.pImageMemoryBarriers = barriers;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depToColor);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vk_math::Mat4 view = vk_math::Mat4::lookAt(vk_math::Vec3(0.0f, 0.8f, 2.5f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

            // =========================================================
            // PART 1: DYNAMIC RENDERING SUSPEND PASS (Background Floor)
            // =========================================================
            VkRenderingAttachmentInfo colorAttachment1{};
            colorAttachment1.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment1.imageView = swapchainImageViews[imageIndex];
            colorAttachment1.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment1.clearValue = { { { 0.04f, 0.04f, 0.08f, 1.0f } } };

            VkRenderingAttachmentInfo depthAttachment1{};
            depthAttachment1.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment1.imageView = depthImageView;
            depthAttachment1.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment1.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment1.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment1.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo renderInfo1{};
            renderInfo1.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderInfo1.flags = VK_RENDERING_SUSPENDING_BIT; // SUSPEND PASS
            renderInfo1.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderInfo1.layerCount = 1;
            renderInfo1.colorAttachmentCount = 1;
            renderInfo1.pColorAttachments = &colorAttachment1;
            renderInfo1.pDepthAttachment = &depthAttachment1;

            vk14.vkCmdBeginRendering(commandBuffer, &renderInfo1);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &floorVBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, floorIBuffer, 0, VK_INDEX_TYPE_UINT16);

            PushConstants pc1{ proj * view, Vec4(0.8f, 0.9f, 1.0f, 1.0f) };
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc1);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(floorIndices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer); // Suspended

            // =========================================================
            // PART 2: DYNAMIC RENDERING RESUME PASS (Foreground Crystal)
            // =========================================================
            VkRenderingAttachmentInfo colorAttachment2{};
            colorAttachment2.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment2.imageView = swapchainImageViews[imageIndex];
            colorAttachment2.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment2.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Continues existing content
            colorAttachment2.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            VkRenderingAttachmentInfo depthAttachment2{};
            depthAttachment2.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment2.imageView = depthImageView;
            depthAttachment2.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment2.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            depthAttachment2.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            VkRenderingInfo renderInfo2{};
            renderInfo2.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderInfo2.flags = VK_RENDERING_RESUMING_BIT; // RESUME PASS
            renderInfo2.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderInfo2.layerCount = 1;
            renderInfo2.colorAttachmentCount = 1;
            renderInfo2.pColorAttachments = &colorAttachment2;
            renderInfo2.pDepthAttachment = &depthAttachment2;

            vk14.vkCmdBeginRendering(commandBuffer, &renderInfo2);

            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &crystalVBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, crystalIBuffer, 0, VK_INDEX_TYPE_UINT16);

            vk_math::Mat4 model = vk_math::Mat4::translate(vk_math::Vec3(0.0f, 0.0f, 0.0f)) *
                                 vk_math::Mat4::rotate(time * 1.2f, vk_math::Vec3(0.3f, 1.0f, 0.2f)) *
                                 vk_math::Mat4::rotate(time * 0.6f, vk_math::Vec3(1.0f, 0.0f, 0.0f));

            PushConstants pc2{ proj * view * model, Vec4(1.0f, 1.0f, 1.0f, 1.0f) };
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc2);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(crystalIndices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer); // Completed

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
        profiler.exportFoldedFile("flamegraph_assignment34_dynamic_rendering_suspend_resume.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment34_dynamic_rendering_suspend_resume.html");
        profiler.exportChromeTraceFile("flamegraph_assignment34_dynamic_rendering_suspend_resume.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, floorVBuffer, nullptr);
        vkFreeMemory(device, floorVMem, nullptr);
        vkDestroyBuffer(device, floorIBuffer, nullptr);
        vkFreeMemory(device, floorIMem, nullptr);

        vkDestroyBuffer(device, crystalVBuffer, nullptr);
        vkFreeMemory(device, crystalVMem, nullptr);
        vkDestroyBuffer(device, crystalIBuffer, nullptr);
        vkFreeMemory(device, crystalIMem, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
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

        std::cout << "\nAssignment 34 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 34 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
