// ============================================================================
// Assignment 31: Vulkan 1.4 Maintenance 5 & Maintenance 6
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_KHR_maintenance5 & VK_KHR_maintenance6 (Vulkan 1.4 core standard)
//   - Dynamic index buffer binding with bounds checking (vkCmdBindIndexBuffer2KHR)
//   - Sub-mesh index buffer ranges from a single consolidated index buffer
//   - Dynamic Rendering with Depth Testing
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
    Vec4 colorTint;
};

// Structure holding submesh offset & count inside the unified index buffer
struct SubmeshInfo {
    uint32_t firstIndex;
    uint32_t indexCount;
    VkDeviceSize byteOffset;
    VkDeviceSize byteSize;
};

// Generate 3 distinct submeshes into a single vertex buffer and single index buffer:
// Submesh 0: Cube (36 indices)
// Submesh 1: Pyramid (18 indices)
// Submesh 2: Octahedron (24 indices)
void generateConsolidatedGeometry(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, std::array<SubmeshInfo, 3>& submeshes) {
    // --- Submesh 0: Cube ---
    uint16_t vStart0 = static_cast<uint16_t>(vertices.size());
    uint32_t iStart0 = static_cast<uint32_t>(indices.size());

    // 8 cube corners
    vertices.push_back({ {-0.4f, -0.4f,  0.4f}, {1.0f, 0.2f, 0.2f} });
    vertices.push_back({ { 0.4f, -0.4f,  0.4f}, {1.0f, 0.7f, 0.1f} });
    vertices.push_back({ { 0.4f,  0.4f,  0.4f}, {1.0f, 1.0f, 0.2f} });
    vertices.push_back({ {-0.4f,  0.4f,  0.4f}, {0.2f, 0.9f, 0.3f} });
    vertices.push_back({ {-0.4f, -0.4f, -0.4f}, {0.1f, 0.5f, 1.0f} });
    vertices.push_back({ { 0.4f, -0.4f, -0.4f}, {0.6f, 0.2f, 1.0f} });
    vertices.push_back({ { 0.4f,  0.4f, -0.4f}, {1.0f, 0.2f, 0.8f} });
    vertices.push_back({ {-0.4f,  0.4f, -0.4f}, {0.9f, 0.9f, 0.9f} });

    uint16_t cubeIdx[] = {
        0, 1, 2, 2, 3, 0, // front
        1, 5, 6, 6, 2, 1, // right
        5, 4, 7, 7, 6, 5, // back
        4, 0, 3, 3, 7, 4, // left
        3, 2, 6, 6, 7, 3, // top
        4, 5, 1, 1, 0, 4  // bottom
    };
    for (uint16_t idx : cubeIdx) indices.push_back(vStart0 + idx);

    submeshes[0] = { iStart0, 36, iStart0 * sizeof(uint16_t), 36 * sizeof(uint16_t) };

    // --- Submesh 1: Pyramid ---
    uint16_t vStart1 = static_cast<uint16_t>(vertices.size());
    uint32_t iStart1 = static_cast<uint32_t>(indices.size());

    vertices.push_back({ {-0.45f, -0.4f,  0.45f}, {0.2f, 0.8f, 1.0f} });
    vertices.push_back({ { 0.45f, -0.4f,  0.45f}, {0.3f, 1.0f, 0.6f} });
    vertices.push_back({ { 0.45f, -0.4f, -0.45f}, {1.0f, 0.8f, 0.2f} });
    vertices.push_back({ {-0.45f, -0.4f, -0.45f}, {1.0f, 0.3f, 0.6f} });
    vertices.push_back({ { 0.0f,   0.5f,   0.0f}, {1.0f, 1.0f, 1.0f} }); // Apex

    uint16_t pyrIdx[] = {
        0, 1, 4, // front
        1, 2, 4, // right
        2, 3, 4, // back
        3, 0, 4, // left
        0, 3, 2, 2, 1, 0 // base
    };
    for (uint16_t idx : pyrIdx) indices.push_back(vStart1 + idx);

    submeshes[1] = { iStart1, 18, iStart1 * sizeof(uint16_t), 18 * sizeof(uint16_t) };

    // --- Submesh 2: Octahedron ---
    uint16_t vStart2 = static_cast<uint16_t>(vertices.size());
    uint32_t iStart2 = static_cast<uint32_t>(indices.size());

    vertices.push_back({ { 0.0f,  0.55f,  0.0f}, {1.0f, 0.9f, 0.2f} }); // Top
    vertices.push_back({ {-0.45f, 0.0f,  0.45f}, {0.2f, 0.9f, 1.0f} });
    vertices.push_back({ { 0.45f, 0.0f,  0.45f}, {0.3f, 1.0f, 0.5f} });
    vertices.push_back({ { 0.45f, 0.0f, -0.45f}, {1.0f, 0.3f, 0.8f} });
    vertices.push_back({ {-0.45f, 0.0f, -0.45f}, {0.9f, 0.5f, 0.1f} });
    vertices.push_back({ { 0.0f, -0.55f,  0.0f}, {0.8f, 0.1f, 0.9f} }); // Bottom

    uint16_t octIdx[] = {
        0, 1, 2,  0, 2, 3,  0, 3, 4,  0, 4, 1, // top 4 faces
        5, 2, 1,  5, 3, 2,  5, 4, 3,  5, 1, 4  // bottom 4 faces
    };
    for (uint16_t idx : octIdx) indices.push_back(vStart2 + idx);

    submeshes[2] = { iStart2, 24, iStart2 * sizeof(uint16_t), 24 * sizeof(uint16_t) };
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
    std::cout << " Assignment 31: Vulkan 1.4 Maintenance 5 & Maintenance 6\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: vkCmdBindIndexBuffer2KHR, Dynamic Index Ranges,\n";
    std::cout << "           Bounded Submesh Rendering from Single Unified Buffer\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 31: Maintenance 5 & 6 (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        // Enable Maintenance 5 Features
        VkPhysicalDeviceMaintenance5FeaturesKHR maint5Features{};
        maint5Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
        maint5Features.maintenance5 = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &maint5Features;
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

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_5_EXTENSION_NAME
        };

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

        auto pfn_vkCmdBindIndexBuffer2KHR = (PFN_vkCmdBindIndexBuffer2KHR)vkGetDeviceProcAddr(device, "vkCmdBindIndexBuffer2KHR");

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

        // Load Shaders
        auto vertCode = vulkan_utils::readFile("shaders/maint_scene.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/maint_scene.frag.spv");

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

        // Consolidated Geometry Setup (3 submeshes in 1 buffer)
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        std::array<SubmeshInfo, 3> submeshes;
        generateConsolidatedGeometry(vertices, indices, submeshes);

        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);
        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        VkBuffer unifiedIndexBuffer;
        VkDeviceMemory unifiedIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     unifiedIndexBuffer, unifiedIndexBufferMemory);
        void* iData;
        vkMapMemory(device, unifiedIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, unifiedIndexBufferMemory);

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

        std::cout << "[Render Loop] Rendering 3 submeshes (Cube, Pyramid, Octahedron) using vkCmdBindIndexBuffer2KHR dynamic index bounds...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment31_maintenance5_maintenance6
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment31_maintenance5_maintenance6");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment31_maintenance5_maintenance6");
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

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = { { { 0.05f, 0.05f, 0.08f, 1.0f } } };

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

            vk_math::Mat4 view = vk_math::Mat4::lookAt(vk_math::Vec3(0.0f, 0.0f, 3.2f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

            // DRAW 3 SUBMESHES USING DYNAMIC INDEX BUFFER RANGES
            float positionsX[3] = { -1.3f, 0.0f, 1.3f };
            Vec4 tints[3] = {
                Vec4(1.0f, 0.4f, 0.4f, 1.0f), // Red
                Vec4(0.3f, 1.0f, 0.5f, 1.0f), // Green
                Vec4(0.3f, 0.7f, 1.0f, 1.0f)  // Blue
            };

            for (uint32_t s = 0; s < 3; ++s) {
                // Maintenance 5: Bind index buffer with explicit byte offset and size bound
                if (pfn_vkCmdBindIndexBuffer2KHR) {
                    pfn_vkCmdBindIndexBuffer2KHR(
                        commandBuffer,
                        unifiedIndexBuffer,
                        submeshes[s].byteOffset,
                        submeshes[s].byteSize,
                        VK_INDEX_TYPE_UINT16
                    );
                    // Draw with indexOffset 0 because the buffer binding offset was adjusted dynamically!
                    vk_math::Mat4 model = vk_math::Mat4::translate(vk_math::Vec3(positionsX[s], 0.0f, 0.0f)) *
                                         vk_math::Mat4::rotate(time * (0.8f + s * 0.3f), vk_math::Vec3(0.5f, 1.0f, 0.2f));
                    PushConstants pc{ proj * view * model, tints[s] };
                    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
                    vkCmdDrawIndexed(commandBuffer, submeshes[s].indexCount, 1, 0, 0, 0);
                } else {
                    vkCmdBindIndexBuffer(commandBuffer, unifiedIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
                    vk_math::Mat4 model = vk_math::Mat4::translate(vk_math::Vec3(positionsX[s], 0.0f, 0.0f)) *
                                         vk_math::Mat4::rotate(time * (0.8f + s * 0.3f), vk_math::Vec3(0.5f, 1.0f, 0.2f));
                    PushConstants pc{ proj * view * model, tints[s] };
                    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
                    vkCmdDrawIndexed(commandBuffer, submeshes[s].indexCount, 1, submeshes[s].firstIndex, 0, 0);
                }
            }

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
        profiler.exportFoldedFile("flamegraph_assignment31_maintenance5_maintenance6.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment31_maintenance5_maintenance6.html");
        profiler.exportChromeTraceFile("flamegraph_assignment31_maintenance5_maintenance6.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, unifiedIndexBuffer, nullptr);
        vkFreeMemory(device, unifiedIndexBufferMemory, nullptr);

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

        std::cout << "\nAssignment 31 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 31 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
