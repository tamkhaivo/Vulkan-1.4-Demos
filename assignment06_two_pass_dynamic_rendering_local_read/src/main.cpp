// ============================================================================
// Assignment 6: Two-Pass Effect with Dynamic Rendering Local Reads
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Vulkan 1.4 Core Dynamic Rendering Local Reads (dynamicRenderingLocalRead feature)
//   - VkRenderingInputAttachmentIndexInfoKHR / VkRenderingAttachmentLocationInfoKHR
//   - subpassInput / subpassLoad shader interfaces without legacy VkRenderPass
//   - Pass 1: 3D Lit Animated Object rendered to offscreen color attachment
//   - PipelineBarrier2 with VK_DEPENDENCY_BY_REGION_BIT & RENDERING_LOCAL_READ_BIT
//   - Pass 2: Fullscreen post-processing shader with dynamic local read sampling
//   - Vulkan 1.4 Dynamic Rendering & Synchronization2
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <filesystem>
#include <chrono>
#include <cmath>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Vertex & Uniform Data Structures
// ----------------------------------------------------------------------------
struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 color;
    vk_math::Vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

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

        // Normal (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        return attributeDescriptions;
    }
};

struct SceneUBO {
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
};

struct PostProcessPushConstants {
    float time;
    float blurRadius;
    float vignetteStrength;
    uint32_t effectMode;
};

// ----------------------------------------------------------------------------
// Geometry Generation (Smooth Lit 3D Cube)
// ----------------------------------------------------------------------------
void generateCube(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    vertices = {
        // Front face (Z = 0.5) - Red / Orange
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.25f, 0.25f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.55f, 0.1f},  {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.85f, 0.1f},  {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.9f, 0.15f, 0.2f},  {0.0f, 0.0f, 1.0f}},

        // Back face (Z = -0.5) - Cyan / Blue
        {{ 0.5f, -0.5f, -0.5f}, {0.1f, 0.6f, 1.0f},   {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.1f, 0.85f, 0.95f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.2f, 0.3f, 0.95f},  {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.1f, 0.45f, 0.85f}, {0.0f, 0.0f, -1.0f}},

        // Top face (Y = -0.5) - Green / Emerald
        {{-0.5f, -0.5f, -0.5f}, {0.2f, 0.95f, 0.3f},  {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.8f, 0.95f, 0.2f},  {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.3f, 1.0f, 0.5f},   {0.0f, -1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.5f, 0.9f, 0.3f},   {0.0f, -1.0f, 0.0f}},

        // Bottom face (Y = 0.5) - Magenta / Purple
        {{-0.5f,  0.5f,  0.5f}, {0.85f, 0.15f, 0.85f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.65f, 0.05f, 0.75f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.95f, 0.25f, 0.75f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.55f, 0.0f, 0.55f},  {0.0f, 1.0f, 0.0f}},

        // Right face (X = 0.5) - Turquoise
        {{ 0.5f, -0.5f,  0.5f}, {0.1f, 0.95f, 0.85f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.85f, 0.65f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.25f, 0.75f, 0.75f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.15f, 0.95f, 0.95f}, {1.0f, 0.0f, 0.0f}},

        // Left face (X = -0.5) - Gold / Amber
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.85f, 0.0f},  {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.95f, 0.75f, 0.2f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.85f, 0.65f, 0.1f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.95f, 0.35f}, {-1.0f, 0.0f, 0.0f}}
    };

    indices = {
        0,  1,  2,      2,  3,  0,    // Front
        4,  5,  6,      6,  7,  4,    // Back
        8,  9, 10,     10, 11,  8,    // Top
       12, 13, 14,     14, 15, 12,    // Bottom
       16, 17, 18,     18, 19, 16,    // Right
       20, 21, 22,     22, 23, 20     // Left
    };
}

// ----------------------------------------------------------------------------
// Buffer & Memory Helper Functions
// ----------------------------------------------------------------------------
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

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
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

// ----------------------------------------------------------------------------
// Custom Device Creation with Dynamic Rendering Local Read Enabled
// ----------------------------------------------------------------------------
VkDevice createDeviceWithLocalRead(VkPhysicalDevice physicalDevice, uint32_t& graphicsQueueFamily) {
    graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Vulkan 1.4 Core Dynamic Rendering Local Read Features
    VkPhysicalDeviceDynamicRenderingLocalReadFeatures localReadFeatures{};
    localReadFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES;
    localReadFeatures.dynamicRenderingLocalRead = VK_TRUE;

    // Vulkan 1.3 Core Features (Dynamic Rendering & Synchronization2)
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &localReadFeatures;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;

    // Vulkan 1.4 Core Features
    VkPhysicalDeviceVulkan14Features vulkan14Features{};
    vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    vulkan14Features.pNext = &vulkan13Features;
    vulkan14Features.vertexAttributeInstanceRateDivisor = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &vulkan14Features;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device;
    vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create Vulkan 1.4 Logical Device with Dynamic Rendering Local Read");
    return device;
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 6: Two-Pass Dynamic Rendering Local Reads (Vulkan 1.4)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: Vulkan 1.4 Dynamic Rendering Local Reads, subpassLoad, Attachment Barriers" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        // STEP 1: Window & Vulkan 1.4 Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 6: Dynamic Rendering Local Reads (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // STEP 2: Device with Dynamic Rendering Local Read feature enabled
        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = createDeviceWithLocalRead(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        PFN_vkCmdSetRenderingAttachmentLocations pfnSetRenderingAttachmentLocations = 
            (PFN_vkCmdSetRenderingAttachmentLocations)vkGetDeviceProcAddr(device, "vkCmdSetRenderingAttachmentLocations");
        if (!pfnSetRenderingAttachmentLocations) {
            pfnSetRenderingAttachmentLocations = (PFN_vkCmdSetRenderingAttachmentLocations)vkGetDeviceProcAddr(device, "vkCmdSetRenderingAttachmentLocationsKHR");
        }

        PFN_vkCmdSetRenderingInputAttachmentIndices pfnSetRenderingInputAttachmentIndices = 
            (PFN_vkCmdSetRenderingInputAttachmentIndices)vkGetDeviceProcAddr(device, "vkCmdSetRenderingInputAttachmentIndices");
        if (!pfnSetRenderingInputAttachmentIndices) {
            pfnSetRenderingInputAttachmentIndices = (PFN_vkCmdSetRenderingInputAttachmentIndices)vkGetDeviceProcAddr(device, "vkCmdSetRenderingInputAttachmentIndicesKHR");
        }

        std::cout << "Vulkan 1.4 Dynamic Rendering Local Read interfaces initialized." << std::endl;

        // STEP 3: Swapchain Setup (Final Target Presentation)
        VkSurfaceFormatKHR surfaceFormat{VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR};

        VkSwapchainCreateInfoKHR swapchainCreateInfo{};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = 2;
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = {WIDTH, HEIGHT};
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainCreateInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        vk_common::check_vk_result(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain), "Failed to create swapchain");

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
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain image view");
        }

        // STEP 4: Offscreen Color Attachment (Rendered in Pass 1, Read locally in Pass 2)
        VkFormat offscreenColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkImage offscreenColorImage;
        VkDeviceMemory offscreenColorMemory;
        VkImageView offscreenColorImageView;

        VkImageCreateInfo offscreenColorInfo{};
        offscreenColorInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        offscreenColorInfo.imageType = VK_IMAGE_TYPE_2D;
        offscreenColorInfo.extent = {WIDTH, HEIGHT, 1};
        offscreenColorInfo.mipLevels = 1;
        offscreenColorInfo.arrayLayers = 1;
        offscreenColorInfo.format = offscreenColorFormat;
        offscreenColorInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        offscreenColorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        offscreenColorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        offscreenColorInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        offscreenColorInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vk_common::check_vk_result(vkCreateImage(device, &offscreenColorInfo, nullptr, &offscreenColorImage), "Failed to create offscreen color image");

        VkMemoryRequirements offscreenMemReqs;
        vkGetImageMemoryRequirements(device, offscreenColorImage, &offscreenMemReqs);

        VkMemoryAllocateInfo offscreenAllocInfo{};
        offscreenAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        offscreenAllocInfo.allocationSize = offscreenMemReqs.size;
        offscreenAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, offscreenMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vk_common::check_vk_result(vkAllocateMemory(device, &offscreenAllocInfo, nullptr, &offscreenColorMemory), "Failed to allocate offscreen color memory");
        vk_common::check_vk_result(vkBindImageMemory(device, offscreenColorImage, offscreenColorMemory, 0), "Failed to bind offscreen color memory");

        VkImageViewCreateInfo offscreenViewInfo{};
        offscreenViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        offscreenViewInfo.image = offscreenColorImage;
        offscreenViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        offscreenViewInfo.format = offscreenColorFormat;
        offscreenViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        offscreenViewInfo.subresourceRange.baseMipLevel = 0;
        offscreenViewInfo.subresourceRange.levelCount = 1;
        offscreenViewInfo.subresourceRange.baseArrayLayer = 0;
        offscreenViewInfo.subresourceRange.layerCount = 1;

        vk_common::check_vk_result(vkCreateImageView(device, &offscreenViewInfo, nullptr, &offscreenColorImageView), "Failed to create offscreen color view");

        // STEP 5: Depth Buffer for Pass 1 (D32_SFLOAT)
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent = {WIDTH, HEIGHT, 1};
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

        vk_common::check_vk_result(vkAllocateMemory(device, &depthAllocInfo, nullptr, &depthImageMemory), "Failed to allocate depth image memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth image memory");

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

        // STEP 6: Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 7: 3D Geometry Buffers (Pass 1 Cube)
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        generateCube(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexBufferMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);

        VkDeviceSize indexBufferSize = sizeof(uint16_t) * indices.size();
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexBufferMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 8: Scene Uniform Buffer Setup (Pass 1)
        VkDeviceSize sceneUBOSize = sizeof(SceneUBO);
        VkBuffer sceneUBOBuffer;
        VkDeviceMemory sceneUBOBufferMemory;
        void* sceneUBOMapped = nullptr;

        createBuffer(device, physicalDevice, sceneUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sceneUBOBuffer, sceneUBOBufferMemory);
        vkMapMemory(device, sceneUBOBufferMemory, 0, sceneUBOSize, 0, &sceneUBOMapped);

        // STEP 9: Descriptor Sets
        // Pass 1 Layout: UBO at binding 0
        VkDescriptorSetLayoutBinding pass1Binding{};
        pass1Binding.binding = 0;
        pass1Binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pass1Binding.descriptorCount = 1;
        pass1Binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo pass1LayoutInfo{};
        pass1LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pass1LayoutInfo.bindingCount = 1;
        pass1LayoutInfo.pBindings = &pass1Binding;

        VkDescriptorSetLayout pass1DescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &pass1LayoutInfo, nullptr, &pass1DescriptorSetLayout), "Failed to create pass 1 descriptor set layout");

        // Pass 2 Layout: Input Attachment at binding 0
        VkDescriptorSetLayoutBinding pass2Binding{};
        pass2Binding.binding = 0;
        pass2Binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        pass2Binding.descriptorCount = 1;
        pass2Binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo pass2LayoutInfo{};
        pass2LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        pass2LayoutInfo.bindingCount = 1;
        pass2LayoutInfo.pBindings = &pass2Binding;

        VkDescriptorSetLayout pass2DescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &pass2LayoutInfo, nullptr, &pass2DescriptorSetLayout), "Failed to create pass 2 descriptor set layout");

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();
        descPoolInfo.maxSets = 2;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        // Allocate Pass 1 Descriptor Set
        VkDescriptorSetAllocateInfo pass1AllocInfo{};
        pass1AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pass1AllocInfo.descriptorPool = descriptorPool;
        pass1AllocInfo.descriptorSetCount = 1;
        pass1AllocInfo.pSetLayouts = &pass1DescriptorSetLayout;

        VkDescriptorSet pass1DescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &pass1AllocInfo, &pass1DescriptorSet), "Failed to allocate pass 1 descriptor set");

        VkDescriptorBufferInfo uboBufferInfo{};
        uboBufferInfo.buffer = sceneUBOBuffer;
        uboBufferInfo.offset = 0;
        uboBufferInfo.range = sizeof(SceneUBO);

        VkWriteDescriptorSet pass1Write{};
        pass1Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pass1Write.dstSet = pass1DescriptorSet;
        pass1Write.dstBinding = 0;
        pass1Write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pass1Write.descriptorCount = 1;
        pass1Write.pBufferInfo = &uboBufferInfo;

        vkUpdateDescriptorSets(device, 1, &pass1Write, 0, nullptr);

        // Allocate Pass 2 Descriptor Set (Local Read Input Attachment)
        VkDescriptorSetAllocateInfo pass2AllocInfo{};
        pass2AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        pass2AllocInfo.descriptorPool = descriptorPool;
        pass2AllocInfo.descriptorSetCount = 1;
        pass2AllocInfo.pSetLayouts = &pass2DescriptorSetLayout;

        VkDescriptorSet pass2DescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &pass2AllocInfo, &pass2DescriptorSet), "Failed to allocate pass 2 descriptor set");

        VkDescriptorImageInfo inputAttachmentImageInfo{};
        inputAttachmentImageInfo.sampler = VK_NULL_HANDLE;
        inputAttachmentImageInfo.imageView = offscreenColorImageView;
        inputAttachmentImageInfo.imageLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;

        VkWriteDescriptorSet pass2Write{};
        pass2Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pass2Write.dstSet = pass2DescriptorSet;
        pass2Write.dstBinding = 0;
        pass2Write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        pass2Write.descriptorCount = 1;
        pass2Write.pImageInfo = &inputAttachmentImageInfo;

        vkUpdateDescriptorSets(device, 1, &pass2Write, 0, nullptr);

        // STEP 10: Pipeline Layouts
        // Pass 1 Layout
        VkPipelineLayoutCreateInfo pass1PipeLayoutInfo{};
        pass1PipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pass1PipeLayoutInfo.setLayoutCount = 1;
        pass1PipeLayoutInfo.pSetLayouts = &pass1DescriptorSetLayout;

        VkPipelineLayout pass1PipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pass1PipeLayoutInfo, nullptr, &pass1PipelineLayout), "Failed to create pass 1 pipeline layout");

        // Pass 2 Layout with Push Constants
        VkPushConstantRange postPushRange{};
        postPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        postPushRange.offset = 0;
        postPushRange.size = sizeof(PostProcessPushConstants);

        VkPipelineLayoutCreateInfo pass2PipeLayoutInfo{};
        pass2PipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pass2PipeLayoutInfo.setLayoutCount = 1;
        pass2PipeLayoutInfo.pSetLayouts = &pass2DescriptorSetLayout;
        pass2PipeLayoutInfo.pushConstantRangeCount = 1;
        pass2PipeLayoutInfo.pPushConstantRanges = &postPushRange;

        VkPipelineLayout pass2PipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pass2PipeLayoutInfo, nullptr, &pass2PipelineLayout), "Failed to create pass 2 pipeline layout");

        // STEP 11: Shader Modules & Pipelines
        // Pass 1 Shaders
        std::string sceneVertPath = "shaders/scene.vert.spv";
        std::string sceneFragPath = "shaders/scene.frag.spv";
        if (!std::filesystem::exists(sceneVertPath)) sceneVertPath = "assignment06_two_pass_dynamic_rendering_local_read/shaders/scene.vert.spv";
        if (!std::filesystem::exists(sceneFragPath)) sceneFragPath = "assignment06_two_pass_dynamic_rendering_local_read/shaders/scene.frag.spv";

        auto sceneVertCode = vulkan_utils::readFile(sceneVertPath);
        auto sceneFragCode = vulkan_utils::readFile(sceneFragPath);

        VkShaderModule sceneVertModule = vulkan_utils::createShaderModule(device, sceneVertCode);
        VkShaderModule sceneFragModule = vulkan_utils::createShaderModule(device, sceneFragCode);

        // Pass 2 Shaders
        std::string postVertPath = "shaders/postprocess.vert.spv";
        std::string postFragPath = "shaders/postprocess.frag.spv";
        if (!std::filesystem::exists(postVertPath)) postVertPath = "assignment06_two_pass_dynamic_rendering_local_read/shaders/postprocess.vert.spv";
        if (!std::filesystem::exists(postFragPath)) postFragPath = "assignment06_two_pass_dynamic_rendering_local_read/shaders/postprocess.frag.spv";

        auto postVertCode = vulkan_utils::readFile(postVertPath);
        auto postFragCode = vulkan_utils::readFile(postFragPath);

        VkShaderModule postVertModule = vulkan_utils::createShaderModule(device, postVertCode);
        VkShaderModule postFragModule = vulkan_utils::createShaderModule(device, postFragCode);

        // Graphics Pipeline 1: 3D Scene to Offscreen Attachment
        VkPipelineShaderStageCreateInfo pass1ShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, sceneVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, sceneFragModule, "main", nullptr}
        };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo pass1VertexInput{};
        pass1VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pass1VertexInput.vertexBindingDescriptionCount = 1;
        pass1VertexInput.pVertexBindingDescriptions = &bindingDescription;
        pass1VertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        pass1VertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

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

        VkPipelineColorBlendAttachmentState pass1ColorBlend{};
        pass1ColorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo pass1Blending{};
        pass1Blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pass1Blending.attachmentCount = 1;
        pass1Blending.pAttachments = &pass1ColorBlend;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo pass1RenderingInfo{};
        pass1RenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pass1RenderingInfo.colorAttachmentCount = 1;
        pass1RenderingInfo.pColorAttachmentFormats = &offscreenColorFormat;
        pass1RenderingInfo.depthAttachmentFormat = depthFormat;

        VkGraphicsPipelineCreateInfo pass1PipelineInfo{};
        pass1PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pass1PipelineInfo.pNext = &pass1RenderingInfo;
        pass1PipelineInfo.stageCount = 2;
        pass1PipelineInfo.pStages = pass1ShaderStages;
        pass1PipelineInfo.pVertexInputState = &pass1VertexInput;
        pass1PipelineInfo.pInputAssemblyState = &inputAssembly;
        pass1PipelineInfo.pViewportState = &viewportState;
        pass1PipelineInfo.pRasterizationState = &rasterizer;
        pass1PipelineInfo.pMultisampleState = &multisampling;
        pass1PipelineInfo.pDepthStencilState = &depthStencil;
        pass1PipelineInfo.pColorBlendState = &pass1Blending;
        pass1PipelineInfo.pDynamicState = &dynamicState;
        pass1PipelineInfo.layout = pass1PipelineLayout;

        VkPipeline pass1Pipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pass1PipelineInfo, nullptr, &pass1Pipeline), "Failed to create pass 1 graphics pipeline");

        // Graphics Pipeline 2: Fullscreen Post-Process with Dynamic Rendering Local Read
        VkPipelineShaderStageCreateInfo pass2ShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, postVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, postFragModule, "main", nullptr}
        };

        // Fullscreen triangle generated directly from gl_VertexIndex (no vertex buffer bound)
        VkPipelineVertexInputStateCreateInfo pass2VertexInput{};
        pass2VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineRasterizationStateCreateInfo postRasterizer = rasterizer;
        postRasterizer.cullMode = VK_CULL_MODE_NONE;

        VkPipelineDepthStencilStateCreateInfo postDepthStencil{};
        postDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        postDepthStencil.depthTestEnable = VK_FALSE;
        postDepthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState pass2ColorBlend{};
        pass2ColorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo pass2Blending{};
        pass2Blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pass2Blending.attachmentCount = 1;
        pass2Blending.pAttachments = &pass2ColorBlend;

        // Dynamic Rendering Local Read input attachment mapping in pipeline
        uint32_t colorAttachmentInputIndex = 0;
        VkRenderingInputAttachmentIndexInfo inputAttachmentIndexInfo{};
        inputAttachmentIndexInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO;
        inputAttachmentIndexInfo.colorAttachmentCount = 1;
        inputAttachmentIndexInfo.pColorAttachmentInputIndices = &colorAttachmentInputIndex;

        VkPipelineRenderingCreateInfo pass2RenderingInfo{};
        pass2RenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pass2RenderingInfo.pNext = &inputAttachmentIndexInfo; // Chained dynamic rendering local read indices
        pass2RenderingInfo.colorAttachmentCount = 1;
        pass2RenderingInfo.pColorAttachmentFormats = &surfaceFormat.format;

        VkGraphicsPipelineCreateInfo pass2PipelineInfo{};
        pass2PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pass2PipelineInfo.pNext = &pass2RenderingInfo;
        pass2PipelineInfo.stageCount = 2;
        pass2PipelineInfo.pStages = pass2ShaderStages;
        pass2PipelineInfo.pVertexInputState = &pass2VertexInput;
        pass2PipelineInfo.pInputAssemblyState = &inputAssembly;
        pass2PipelineInfo.pViewportState = &viewportState;
        pass2PipelineInfo.pRasterizationState = &postRasterizer;
        pass2PipelineInfo.pMultisampleState = &multisampling;
        pass2PipelineInfo.pDepthStencilState = &postDepthStencil;
        pass2PipelineInfo.pColorBlendState = &pass2Blending;
        pass2PipelineInfo.pDynamicState = &dynamicState;
        pass2PipelineInfo.layout = pass2PipelineLayout;

        VkPipeline pass2Pipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pass2PipelineInfo, nullptr, &pass2Pipeline), "Failed to create pass 2 graphics pipeline");

        // STEP 12: Command Buffers & Sync
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer), "Failed to allocate command buffer");

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence), "Failed to create fence");

        std::cout << "Two-Pass Dynamic Rendering Pipeline with Local Reads initialized successfully. Entering render loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        // STEP 13: Render Loop
        
        // Initialize Flame Graph Profiler for assignment06_two_pass_dynamic_rendering_local_read
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment06_two_pass_dynamic_rendering_local_read");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment06_two_pass_dynamic_rendering_local_read");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Update 3D Model View Projection Matrices
            SceneUBO ubo{};
            ubo.model = vk_math::Mat4::rotate(time * 0.8f, vk_math::Vec3(0.5f, 1.0f, 0.2f));
            ubo.view = vk_math::Mat4::lookAt(
                vk_math::Vec3(0.0f, 0.0f, 2.8f),
                vk_math::Vec3(0.0f, 0.0f, 0.0f),
                vk_math::Vec3(0.0f, 1.0f, 0.0f)
            );
            ubo.proj = vk_math::Mat4::perspective(
                vk_math::radians(45.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.1f,
                100.0f
            );
            std::memcpy(sceneUBOMapped, &ubo, sizeof(ubo));

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // ================================================================
            // PASS 1: Render 3D Lit Geometry to Offscreen Color Attachment
            // ================================================================
            // Transition offscreen color attachment to COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                offscreenColorImage,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Transition depth image to DEPTH_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                depthImage,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );

            VkRenderingAttachmentInfo pass1ColorAttachment{};
            pass1ColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass1ColorAttachment.imageView = offscreenColorImageView;
            pass1ColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass1ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass1ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass1ColorAttachment.clearValue.color = {{0.04f, 0.05f, 0.08f, 1.0f}};

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo pass1Rendering{};
            pass1Rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            pass1Rendering.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            pass1Rendering.layerCount = 1;
            pass1Rendering.colorAttachmentCount = 1;
            pass1Rendering.pColorAttachments = &pass1ColorAttachment;
            pass1Rendering.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &pass1Rendering);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass1Pipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass1PipelineLayout, 0, 1, &pass1DescriptorSet, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // ================================================================
            // INTER-PASS SYNCHRONIZATION: PipelineBarrier2 for Dynamic Local Read
            // Synchronize attachment write in Pass 1 to local input read in Pass 2
            // using VK_DEPENDENCY_BY_REGION_BIT and RENDERING_LOCAL_READ layout
            // ================================================================
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                offscreenColorImage,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Transition Swapchain presentation image to COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // ================================================================
            // PASS 2: Fullscreen Post-Process with Dynamic Rendering Local Read
            // ================================================================
            VkRenderingAttachmentInfo pass2ColorAttachment{};
            pass2ColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass2ColorAttachment.imageView = swapchainImageViews[imageIndex];
            pass2ColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass2ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass2ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass2ColorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

            VkRenderingInfo pass2Rendering{};
            pass2Rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            pass2Rendering.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            pass2Rendering.layerCount = 1;
            pass2Rendering.colorAttachmentCount = 1;
            pass2Rendering.pColorAttachments = &pass2ColorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &pass2Rendering);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass2Pipeline);

            if (pfnSetRenderingInputAttachmentIndices) {
                uint32_t localReadInputIdx = 0;
                VkRenderingInputAttachmentIndexInfo dynInputIdxInfo{};
                dynInputIdxInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO;
                dynInputIdxInfo.colorAttachmentCount = 1;
                dynInputIdxInfo.pColorAttachmentInputIndices = &localReadInputIdx;
                pfnSetRenderingInputAttachmentIndices(commandBuffer, &dynInputIdxInfo);
            }

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Bind Pass 2 Descriptor Set containing the Dynamic Rendering Local Read Input Attachment
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass2PipelineLayout, 0, 1, &pass2DescriptorSet, 0, nullptr);

            PostProcessPushConstants postConsts{};
            postConsts.time = time;
            postConsts.blurRadius = 1.0f;
            postConsts.vignetteStrength = 1.0f;
            postConsts.effectMode = 0; // Cinematic Tone-Mapped Bloom & Vignette

            vkCmdPushConstants(commandBuffer, pass2PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessPushConstants), &postConsts);

            // Draw fullscreen triangle (3 vertices, 1 instance)
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // Transition Swapchain Image for Presentation
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vkEndCommandBuffer(commandBuffer);

            // Submit Command Buffer
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

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

            // Present Image
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment06_two_pass_dynamic_rendering_local_read.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment06_two_pass_dynamic_rendering_local_read.html");
        profiler.exportChromeTraceFile("flamegraph_assignment06_two_pass_dynamic_rendering_local_read.json");
        profiler.cleanupGpu();


        // STEP 14: Cleanup Resources
        vkUnmapMemory(device, sceneUBOBufferMemory);
        vkDestroyBuffer(device, sceneUBOBuffer, nullptr);
        vkFreeMemory(device, sceneUBOBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, pass1DescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, pass2DescriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyImageView(device, offscreenColorImageView, nullptr);
        vkDestroyImage(device, offscreenColorImage, nullptr);
        vkFreeMemory(device, offscreenColorMemory, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipeline(device, pass1Pipeline, nullptr);
        vkDestroyPipelineLayout(device, pass1PipelineLayout, nullptr);
        vkDestroyPipeline(device, pass2Pipeline, nullptr);
        vkDestroyPipelineLayout(device, pass2PipelineLayout, nullptr);

        vkDestroyShaderModule(device, sceneVertModule, nullptr);
        vkDestroyShaderModule(device, sceneFragModule, nullptr);
        vkDestroyShaderModule(device, postVertModule, nullptr);
        vkDestroyShaderModule(device, postFragModule, nullptr);

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

    std::cout << "Assignment 6 finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
