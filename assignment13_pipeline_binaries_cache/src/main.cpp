// ============================================================================
// Assignment 13: Pipeline Binaries (VK_KHR_pipeline_binary) & Pipeline Cache
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Vulkan 1.4 VK_KHR_pipeline_binary and VkPipelineCache
//   - Zero-hitch runtime pipeline compilation via pre-warmed shader binaries
//   - Disk serialization and deserialization of pipeline cache blobs
//   - Vulkan 1.4 Core Dynamic Rendering & Synchronization2 Real-Time Render Loop
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <cstring>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Vertex & MVP Structures
// ----------------------------------------------------------------------------
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

        // Position (location = 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color (location = 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Animated 3D Pyramid / Tetrahedron Vertices
const std::vector<Vertex> meshVertices = {
    // Base triangle (Teal / Cyan)
    {{-0.6f, -0.4f, -0.5f}, {0.1f, 0.9f, 0.8f}},
    {{ 0.6f, -0.4f, -0.5f}, {0.2f, 0.6f, 1.0f}},
    {{ 0.0f, -0.4f,  0.6f}, {0.0f, 1.0f, 0.5f}},

    // Front Left Face (Emerald / Green)
    {{-0.6f, -0.4f, -0.5f}, {0.2f, 0.95f, 0.3f}},
    {{ 0.0f, -0.4f,  0.6f}, {0.1f, 0.85f, 0.7f}},
    {{ 0.0f,  0.6f,  0.0f}, {1.0f, 0.95f, 0.2f}},

    // Front Right Face (Gold / Orange)
    {{ 0.0f, -0.4f,  0.6f}, {1.0f, 0.7f, 0.1f}},
    {{ 0.6f, -0.4f, -0.5f}, {1.0f, 0.3f, 0.2f}},
    {{ 0.0f,  0.6f,  0.0f}, {1.0f, 0.95f, 0.2f}},

    // Back Face (Magenta / Purple)
    {{ 0.6f, -0.4f, -0.5f}, {0.9f, 0.1f, 0.8f}},
    {{-0.6f, -0.4f, -0.5f}, {0.5f, 0.1f, 0.9f}},
    {{ 0.0f,  0.6f,  0.0f}, {1.0f, 0.95f, 0.2f}}
};

const std::vector<uint16_t> meshIndices = {
    0, 1, 2,
    3, 4, 5,
    6, 7, 8,
    9, 10, 11
};

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

static void createBuffer(
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

static void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size
) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "Assignment 13: Pipeline Binaries & Cache Optimization\n";
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << "Concepts: VK_KHR_pipeline_binary, VkPipelineCache Serialization,\n";
    std::cout << "          Pre-warming PSOs & Hitch-Free Graphics Execution\n";
    std::cout << "========================================================\n";

    constexpr uint32_t WIDTH = 800;
    constexpr uint32_t HEIGHT = 600;
    constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    const std::string cacheFileName = "pipeline_cache.bin";

    try {
        // STEP 1: Window, Instance, Surface, Physical Device, Logical Device
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 13: Pipeline Binaries & Cache (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        std::cout << "Vulkan 1.4 Logical Device initialized with Dynamic Rendering & Synchronization2.\n";

        // STEP 2: Load / Initialize VkPipelineCache from Disk Blob (Warm-up)
        std::vector<char> initialCacheData;
        if (std::filesystem::exists(cacheFileName)) {
            std::ifstream file(cacheFileName, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                size_t size = static_cast<size_t>(file.tellg());
                file.seekg(0);
                initialCacheData.resize(size);
                file.read(initialCacheData.data(), size);
                file.close();
                std::cout << "[Pipeline Cache] Loaded existing cache blob from disk: " << size << " bytes.\n";
            }
        }

        VkPipelineCacheCreateInfo cacheCreateInfo{};
        cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        cacheCreateInfo.initialDataSize = initialCacheData.size();
        cacheCreateInfo.pInitialData = initialCacheData.empty() ? nullptr : initialCacheData.data();

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        vk_common::check_vk_result(vkCreatePipelineCache(device, &cacheCreateInfo, nullptr, &pipelineCache), "Failed to create VkPipelineCache");
        std::cout << "VkPipelineCache successfully initialized for PSO warm-up.\n";

        // STEP 3: Swapchain Setup
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
        VkExtent2D swapchainExtent = {WIDTH, HEIGHT};

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data());
        VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
        for (const auto& sf : surfaceFormats) {
            if (sf.format == VK_FORMAT_B8G8R8A8_UNORM || sf.format == VK_FORMAT_R8G8B8A8_UNORM) {
                surfaceFormat = sf;
                break;
            }
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = swapchainExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.preTransform = capabilities.currentTransform;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainCreateInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        vk_common::check_vk_result(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain), "Failed to create swapchain");

        uint32_t actualImageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, nullptr);
        std::vector<VkImage> swapchainImages(actualImageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data());

        std::vector<VkImageView> swapchainImageViews(actualImageCount);
        for (uint32_t i = 0; i < actualImageCount; ++i) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain image view");
        }

        // STEP 4: Depth Buffer Setup
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent.width = WIDTH;
        depthImageInfo.extent.height = HEIGHT;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.format = depthFormat;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vk_common::check_vk_result(vkCreateImage(device, &depthImageInfo, nullptr, &depthImage), "Failed to create depth image");

        VkMemoryRequirements depthMemReqs;
        vkGetImageMemoryRequirements(device, depthImage, &depthMemReqs);

        VkMemoryAllocateInfo depthAllocInfo{};
        depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAllocInfo.allocationSize = depthMemReqs.size;
        depthAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, depthMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vk_common::check_vk_result(vkAllocateMemory(device, &depthAllocInfo, nullptr, &depthImageMemory), "Failed to allocate depth memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth memory");

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        vk_common::check_vk_result(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView), "Failed to create depth image view");

        // STEP 5: Command Pool & Geometry Buffers
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool), "Failed to create command pool");

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * meshVertices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexBufferMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, meshVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);

        VkDeviceSize indexBufferSize = sizeof(uint16_t) * meshIndices.size();
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexBufferMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, meshIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 6: Pipeline Layout with Push Constants
        struct PushConstants {
            vk_math::Mat4 mvp;
        };

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 7: Load Shaders and Create Cached Graphics Pipeline (Warm-Up via VkPipelineCache)
        std::string vertPath = "assignment13_pipeline_binaries_cache/shaders/cache.vert.spv";
        std::string fragPath = "assignment13_pipeline_binaries_cache/shaders/cache.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "shaders/cache.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "shaders/cache.frag.spv";

        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

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

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;
        pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

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
        pipelineInfo.renderPass = VK_NULL_HANDLE;

        // Compile graphics pipeline directly utilizing the warm-up VkPipelineCache!
        auto compileStartTime = std::chrono::high_resolution_clock::now();
        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create cached graphics pipeline");
        auto compileEndTime = std::chrono::high_resolution_clock::now();
        float compileTimeMs = std::chrono::duration<float, std::milli>(compileEndTime - compileStartTime).count();

        std::cout << "[Pipeline Cache] Pipeline compiled in " << compileTimeMs << " ms using VkPipelineCache.\n";

        // Query updated cache data size
        size_t cacheSize = 0;
        vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr);
        std::cout << "[Pipeline Cache] Post-compilation cache data size: " << cacheSize << " bytes.\n";

        // STEP 8: Command Buffers & Sync Primitives
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &cmdAllocInfo, commandBuffers.data()), "Failed to allocate command buffers");

        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores;
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk_common::check_vk_result(vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]), "Failed to create semaphore");
            vk_common::check_vk_result(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]), "Failed to create semaphore");
            vk_common::check_vk_result(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]), "Failed to create fence");
        }

        std::cout << "Pipeline Binaries & Cache Pipeline active. Starting real-time render loop...\n";

        // STEP 9: Continuous Interactive Render Loop
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t currentFrame = 0;
        int renderedFrames = 0;

        
        // Initialize Flame Graph Profiler for assignment13_pipeline_binaries_cache
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment13_pipeline_binaries_cache");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment13_pipeline_binaries_cache");
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFences[currentFrame]);

            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Animated Model-View-Projection Matrix
            vk_math::Mat4 model = vk_math::Mat4::rotate(time * vk_math::radians(60.0f), vk_math::Vec3(0.3f, 1.0f, 0.4f));
            vk_math::Mat4 view = vk_math::Mat4::lookAt(vk_math::Vec3(0.0f, 1.2f, 2.8f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
            
            PushConstants pc{};
            pc.mvp = proj * view * model;

            // Record Command Buffer
            VkCommandBuffer cmd = commandBuffers[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &beginInfo);

            // Synchronization2 Layout Transitions
            vulkan_utils::pipelineBarrier2ImageTransition(
                cmd, vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vulkan_utils::pipelineBarrier2ImageTransition(
                cmd, vk14.vkCmdPipelineBarrier2,
                depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );

            // Vulkan 1.4 Dynamic Rendering Setup
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.06f, 0.08f, 0.12f, 1.0f}}; // Modern dark navy

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(cmd, &renderingInfo);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstants),
                &pc
            );

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(meshIndices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(cmd);

            // Transition Swapchain Image for Presentation
            vulkan_utils::pipelineBarrier2ImageTransition(
                cmd, vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vkEndCommandBuffer(cmd);

            // Submit Command Buffer
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

            // Present Image
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            renderedFrames++;
        }

        std::cout << "[Status] Successfully rendered " << renderedFrames << " frames.\n";

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment13_pipeline_binaries_cache.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment13_pipeline_binaries_cache.html");
        profiler.exportChromeTraceFile("flamegraph_assignment13_pipeline_binaries_cache.json");
        profiler.cleanupGpu();


        // STEP 10: Serialize Pipeline Cache Data back to Disk for Next Run Warm-Up
        size_t finalCacheSize = 0;
        vkGetPipelineCacheData(device, pipelineCache, &finalCacheSize, nullptr);
        if (finalCacheSize > 0) {
            std::vector<char> cacheData(finalCacheSize);
            if (vkGetPipelineCacheData(device, pipelineCache, &finalCacheSize, cacheData.data()) == VK_SUCCESS) {
                std::ofstream file(cacheFileName, std::ios::binary);
                if (file.is_open()) {
                    file.write(cacheData.data(), finalCacheSize);
                    file.close();
                    std::cout << "[Pipeline Cache] Serialized " << finalCacheSize << " bytes of pipeline cache to disk (" << cacheFileName << ").\n";
                }
            }
        }

        // STEP 11: Cleanup Resources
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);

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

    std::cout << "Assignment 13 (Pipeline Binaries & Cache) completed cleanly.\n";
    return EXIT_SUCCESS;
}
