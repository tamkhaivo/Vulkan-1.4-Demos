// ============================================================================
// Assignment 4: Push Constants and Dynamic Uniform Buffers
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
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
#include <cstdlib>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Vertex, Uniform, and Push Constant Data Structures
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

// Static Scene Uniform Buffer Object (Camera View & Projection)
struct SceneUBO {
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
};

// Per-Object Dynamic Material Uniform Buffer Object
struct alignas(16) MaterialData {
    float baseColor[4];         // vec4
    float ambient[4];           // vec4
    float diffuse[4];           // vec4
    float specularRoughness[4]; // vec4 (rgb = specular color, w = shininess)
};

// Push Constants for High-Frequency per-draw Model Transform and Info
struct PushConstantData {
    vk_math::Mat4 model;
    float time;
    uint32_t objectIndex;
    float padding[2]; // align to 16 bytes for safety
};

// Helper: Calculate aligned size for dynamic uniform buffers
static size_t getAlignedSize(size_t size, size_t alignment) {
    if (alignment > 0) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
    return size;
}

// ----------------------------------------------------------------------------
// Geometry Generation (Cube & Pyramids)
// ----------------------------------------------------------------------------

// Generate a 3D Cube with positions, colors, and distinct face normals
void generateCube(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, uint16_t baseIndex) {
    std::vector<Vertex> cubeVerts = {
        // Front face (Z = 0.5) - Normal (0, 0, 1)
        {{-0.5f, -0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 0.0f, 1.0f}},

        // Back face (Z = -0.5) - Normal (0, 0, -1)
        {{ 0.5f, -0.5f, -0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, -1.0f}},

        // Top face (Y = -0.5) - Normal (0, -1, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.95f, 0.95f, 0.95f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.95f, 0.95f, 0.95f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.95f, 0.95f, 0.95f}, {0.0f, -1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.95f, 0.95f, 0.95f}, {0.0f, -1.0f, 0.0f}},

        // Bottom face (Y = 0.5) - Normal (0, 1, 0)
        {{-0.5f,  0.5f,  0.5f}, {0.7f, 0.7f, 0.7f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.7f, 0.7f, 0.7f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.7f, 0.7f, 0.7f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.7f, 0.7f, 0.7f}, {0.0f, 1.0f, 0.0f}},

        // Right face (X = 0.5) - Normal (1, 0, 0)
        {{ 0.5f, -0.5f,  0.5f}, {0.85f, 0.85f, 0.85f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.85f, 0.85f, 0.85f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.85f, 0.85f, 0.85f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.85f, 0.85f, 0.85f}, {1.0f, 0.0f, 0.0f}},

        // Left face (X = -0.5) - Normal (-1, 0, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.75f, 0.75f, 0.75f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.75f, 0.75f, 0.75f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.75f, 0.75f, 0.75f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.75f, 0.75f, 0.75f}, {-1.0f, 0.0f, 0.0f}}
    };

    std::vector<uint16_t> cubeIdxs = {
        0,  1,  2,      2,  3,  0,    // Front
        4,  5,  6,      6,  7,  4,    // Back
        8,  9, 10,     10, 11,  8,    // Top
       12, 13, 14,     14, 15, 12,    // Bottom
       16, 17, 18,     18, 19, 16,    // Right
       20, 21, 22,     22, 23, 20     // Left
    };

    for (const auto& v : cubeVerts) {
        vertices.push_back(v);
    }
    for (auto i : cubeIdxs) {
        indices.push_back(baseIndex + i);
    }
}

// ----------------------------------------------------------------------------
// Helper Functions for Buffers & Memory
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

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 4: Push Constants & Dynamic Uniforms (Vulkan 1.4)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: VkPushConstantRange, vkCmdPushConstants, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        // STEP 1: Window & Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 4: Push Constants & Dynamic Uniforms (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Check physical device limits for dynamic uniform buffer offset alignment
        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
        size_t minUboAlignment = deviceProps.limits.minUniformBufferOffsetAlignment;
        std::cout << "GPU minUniformBufferOffsetAlignment: " << minUboAlignment << " bytes" << std::endl;

        // STEP 2: Logical Device & Vulkan 1.4 Function Pointers
        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                graphicsQueueFamily = i;
                break;
            }
        }

        if (graphicsQueueFamily == UINT32_MAX) {
            throw std::runtime_error("Could not find a queue family supporting graphics and presentation!");
        }

        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (!vk14.vkCmdBeginRendering || !vk14.vkCmdEndRendering || !vk14.vkCmdPipelineBarrier2) {
            throw std::runtime_error("Failed to load Vulkan 1.4 core dynamic rendering function pointers!");
        }

        // STEP 3: Swapchain & Color Image Views
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

            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create color image view");
        }

        // STEP 4: Depth Attachment for 3D Geometry (D32_SFLOAT)
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

        // STEP 5: Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 6: Geometry Buffers (Cube mesh used for all 3D objects)
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        generateCube(vertices, indices, 0);

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

        // STEP 7: Uniform Buffers Setup
        // (A) Static Scene UBO (Binding 0)
        VkDeviceSize sceneUBOSize = sizeof(SceneUBO);
        VkBuffer sceneUBOBuffer;
        VkDeviceMemory sceneUBOBufferMemory;
        void* sceneUBOMapped = nullptr;

        createBuffer(device, physicalDevice, sceneUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sceneUBOBuffer, sceneUBOBufferMemory);
        vkMapMemory(device, sceneUBOBufferMemory, 0, sceneUBOSize, 0, &sceneUBOMapped);

        // (B) Dynamic Material Uniform Buffer (Binding 1)
        // 5 distinct objects in our scene with customized material data
        const uint32_t OBJECT_COUNT = 5;
        size_t dynamicAlignment = getAlignedSize(sizeof(MaterialData), minUboAlignment);
        VkDeviceSize dynamicBufferSize = dynamicAlignment * OBJECT_COUNT;

        VkBuffer dynamicMaterialBuffer;
        VkDeviceMemory dynamicMaterialBufferMemory;
        void* dynamicMaterialMapped = nullptr;

        createBuffer(device, physicalDevice, dynamicBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     dynamicMaterialBuffer, dynamicMaterialBufferMemory);
        vkMapMemory(device, dynamicMaterialBufferMemory, 0, dynamicBufferSize, 0, &dynamicMaterialMapped);

        // Define 5 distinct materials for each object:
        // 0: Cyber Emerald (Green/Cyan metallic)
        // 1: Solar Gold (Warm vibrant gold)
        // 2: Electric Ruby (Vivid crimson/magenta)
        // 3: Deep Sapphire (Cool neon blue)
        // 4: Amethyst Quartz (Rich royal purple)
        std::vector<MaterialData> materials(OBJECT_COUNT);

        // Material 0: Cyber Emerald
        materials[0].baseColor[0] = 0.1f; materials[0].baseColor[1] = 0.95f; materials[0].baseColor[2] = 0.65f; materials[0].baseColor[3] = 1.0f;
        materials[0].ambient[0] = 0.2f; materials[0].ambient[1] = 0.2f; materials[0].ambient[2] = 0.2f; materials[0].ambient[3] = 1.0f;
        materials[0].diffuse[0] = 0.8f; materials[0].diffuse[1] = 0.8f; materials[0].diffuse[2] = 0.8f; materials[0].diffuse[3] = 1.0f;
        materials[0].specularRoughness[0] = 1.0f; materials[0].specularRoughness[1] = 1.0f; materials[0].specularRoughness[2] = 1.0f; materials[0].specularRoughness[3] = 64.0f;

        // Material 1: Solar Gold
        materials[1].baseColor[0] = 1.0f; materials[1].baseColor[1] = 0.75f; materials[1].baseColor[2] = 0.15f; materials[1].baseColor[3] = 1.0f;
        materials[1].ambient[0] = 0.2f; materials[1].ambient[1] = 0.2f; materials[1].ambient[2] = 0.2f; materials[1].ambient[3] = 1.0f;
        materials[1].diffuse[0] = 0.85f; materials[1].diffuse[1] = 0.85f; materials[1].diffuse[2] = 0.85f; materials[1].diffuse[3] = 1.0f;
        materials[1].specularRoughness[0] = 1.0f; materials[1].specularRoughness[1] = 0.9f; materials[1].specularRoughness[2] = 0.5f; materials[1].specularRoughness[3] = 32.0f;

        // Material 2: Electric Ruby
        materials[2].baseColor[0] = 1.0f; materials[2].baseColor[1] = 0.15f; materials[2].baseColor[2] = 0.35f; materials[2].baseColor[3] = 1.0f;
        materials[2].ambient[0] = 0.2f; materials[2].ambient[1] = 0.2f; materials[2].ambient[2] = 0.2f; materials[2].ambient[3] = 1.0f;
        materials[2].diffuse[0] = 0.9f; materials[2].diffuse[1] = 0.9f; materials[2].diffuse[2] = 0.9f; materials[2].diffuse[3] = 1.0f;
        materials[2].specularRoughness[0] = 1.0f; materials[2].specularRoughness[1] = 0.8f; materials[2].specularRoughness[2] = 0.8f; materials[2].specularRoughness[3] = 128.0f;

        // Material 3: Deep Sapphire
        materials[3].baseColor[0] = 0.15f; materials[3].baseColor[1] = 0.45f; materials[3].baseColor[2] = 1.0f; materials[3].baseColor[3] = 1.0f;
        materials[3].ambient[0] = 0.2f; materials[3].ambient[1] = 0.2f; materials[3].ambient[2] = 0.2f; materials[3].ambient[3] = 1.0f;
        materials[3].diffuse[0] = 0.8f; materials[3].diffuse[1] = 0.8f; materials[3].diffuse[2] = 0.8f; materials[3].diffuse[3] = 1.0f;
        materials[3].specularRoughness[0] = 0.8f; materials[3].specularRoughness[1] = 0.9f; materials[3].specularRoughness[2] = 1.0f; materials[3].specularRoughness[3] = 48.0f;

        // Material 4: Amethyst Quartz
        materials[4].baseColor[0] = 0.75f; materials[4].baseColor[1] = 0.2f; materials[4].baseColor[2] = 0.95f; materials[4].baseColor[3] = 1.0f;
        materials[4].ambient[0] = 0.2f; materials[4].ambient[1] = 0.2f; materials[4].ambient[2] = 0.2f; materials[4].ambient[3] = 1.0f;
        materials[4].diffuse[0] = 0.85f; materials[4].diffuse[1] = 0.85f; materials[4].diffuse[2] = 0.85f; materials[4].diffuse[3] = 1.0f;
        materials[4].specularRoughness[0] = 1.0f; materials[4].specularRoughness[1] = 0.7f; materials[4].specularRoughness[2] = 1.0f; materials[4].specularRoughness[3] = 96.0f;

        // Copy initial material data to mapped buffer at proper dynamic offsets
        for (uint32_t i = 0; i < OBJECT_COUNT; ++i) {
            char* dest = static_cast<char*>(dynamicMaterialMapped) + (i * dynamicAlignment);
            std::memcpy(dest, &materials[i], sizeof(MaterialData));
        }

        // STEP 8: Descriptor Set Layout & Descriptors
        // Binding 0: Static Scene UBO (Vertex stage)
        // Binding 1: Dynamic Material UBO (Fragment stage)
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();
        descPoolInfo.maxSets = 1;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        // Descriptor Set Allocation
        VkDescriptorSetAllocateInfo descAllocInfo{};
        descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descAllocInfo.descriptorPool = descriptorPool;
        descAllocInfo.descriptorSetCount = 1;
        descAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &descAllocInfo, &descriptorSet), "Failed to allocate descriptor set");

        // Descriptor Writes
        VkDescriptorBufferInfo sceneBufferInfoDesc{};
        sceneBufferInfoDesc.buffer = sceneUBOBuffer;
        sceneBufferInfoDesc.offset = 0;
        sceneBufferInfoDesc.range = sizeof(SceneUBO);

        VkDescriptorBufferInfo dynamicBufferInfoDesc{};
        dynamicBufferInfoDesc.buffer = dynamicMaterialBuffer;
        dynamicBufferInfoDesc.offset = 0;
        dynamicBufferInfoDesc.range = sizeof(MaterialData); // Size of single dynamic slice

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &sceneBufferInfoDesc;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &dynamicBufferInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        // STEP 9: Pipeline Layout with Push Constant Range
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstantData);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 10: Shaders & Graphics Pipeline
        std::string vertPath = "shaders/object.vert.spv";
        std::string fragPath = "shaders/object.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment04_push_constants_dynamic_uniforms/shaders/object.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment04_push_constants_dynamic_uniforms/shaders/object.frag.spv";

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
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

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

        // Vulkan 1.4 Dynamic Rendering Pipeline Setup (Color + Depth Formats)
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

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 11: Command Buffers & Sync Primitives
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

        std::cout << "Vulkan 1.4 Push Constants & Dynamic Uniforms initialized successfully. Entering render loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        // STEP 12: Render Loop
        
        // Initialize Flame Graph Profiler for assignment04_push_constants_dynamic_uniforms
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment04_push_constants_dynamic_uniforms");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment04_push_constants_dynamic_uniforms");
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

            // (1) Update Static Camera Scene UBO
            SceneUBO sceneUBO{};
            sceneUBO.view = vk_math::Mat4::lookAt(
                vk_math::Vec3(0.0f, 4.0f, 8.5f),  // Eye Position (elevated)
                vk_math::Vec3(0.0f, 0.0f, 0.0f),  // LookAt Center
                vk_math::Vec3(0.0f, 1.0f, 0.0f)   // Up Vector
            );
            sceneUBO.proj = vk_math::Mat4::perspective(
                vk_math::radians(45.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.1f,
                100.0f
            );
            std::memcpy(sceneUBOMapped, &sceneUBO, sizeof(sceneUBO));

            // (2) Update Dynamic Materials (e.g. subtle pulsing roughness/color over time)
            for (uint32_t i = 0; i < OBJECT_COUNT; ++i) {
                MaterialData mat = materials[i];
                // Subtle pulse to specular shininess
                mat.specularRoughness[3] = materials[i].specularRoughness[3] * (0.8f + 0.2f * std::sin(time * 2.0f + i));
                char* dest = static_cast<char*>(dynamicMaterialMapped) + (i * dynamicAlignment);
                std::memcpy(dest, &mat, sizeof(MaterialData));
            }

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // Transition Swapchain Image to COLOR_ATTACHMENT_OPTIMAL via Vulkan 1.4 PipelineBarrier2
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

            // Transition Depth Image to DEPTH_ATTACHMENT_OPTIMAL via Vulkan 1.4 PipelineBarrier2
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

            // Dynamic Rendering Setup
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.05f, 0.06f, 0.09f, 1.0f}};

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

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Render 5 objects fanned out in a dynamic 3D configuration
            // 1 Center object + 4 Orbiting/Surrounding objects
            // Each object updates:
            //   - Push Constant: Model transform matrix (translation + rotation + scale), time, objectIndex
            //   - Dynamic Uniform Buffer: Offset binding to material data slice
            for (uint32_t objIdx = 0; objIdx < OBJECT_COUNT; ++objIdx) {
                // Calculate individual object model transformation
                vk_math::Mat4 model = vk_math::Mat4::identity();

                if (objIdx == 0) {
                    // Center rotating monolith cube
                    model = vk_math::Mat4::translate(vk_math::Vec3(0.0f, std::sin(time * 1.5f) * 0.3f, 0.0f))
                          * vk_math::Mat4::rotate(time * vk_math::radians(45.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f))
                          * vk_math::Mat4::rotate(time * vk_math::radians(25.0f), vk_math::Vec3(1.0f, 0.0f, 0.0f))
                          * vk_math::Mat4::scale(vk_math::Vec3(1.3f, 1.3f, 1.3f));
                } else {
                    // 4 orbiting satellite objects arranged in a circle
                    float angleOffset = (objIdx - 1) * (vk_math::PI * 2.0f / 4.0f);
                    float orbitSpeed = 1.0f + (objIdx * 0.2f);
                    float currentAngle = time * orbitSpeed + angleOffset;
                    float orbitRadius = 3.2f;

                    float posX = std::cos(currentAngle) * orbitRadius;
                    float posZ = std::sin(currentAngle) * orbitRadius;
                    float posY = std::sin(time * 2.0f + objIdx) * 0.6f;

                    model = vk_math::Mat4::translate(vk_math::Vec3(posX, posY, posZ))
                          * vk_math::Mat4::rotate(time * vk_math::radians(75.0f * objIdx), vk_math::Vec3(1.0f, 1.0f, 0.5f))
                          * vk_math::Mat4::scale(vk_math::Vec3(0.75f, 0.75f, 0.75f));
                }

                // 1. Send Push Constants (High frequency per-draw model transform & metadata)
                PushConstantData pc{};
                pc.model = model;
                pc.time = time;
                pc.objectIndex = objIdx;

                vkCmdPushConstants(
                    commandBuffer,
                    pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(PushConstantData),
                    &pc
                );

                // 2. Bind Descriptor Set with Dynamic Offset for Material UBO
                uint32_t dynamicOffset = static_cast<uint32_t>(objIdx * dynamicAlignment);
                vkCmdBindDescriptorSets(
                    commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout,
                    0,
                    1,
                    &descriptorSet,
                    1,
                    &dynamicOffset
                );

                // 3. Draw Object Mesh
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            }

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
        profiler.exportFoldedFile("flamegraph_assignment04_push_constants_dynamic_uniforms.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment04_push_constants_dynamic_uniforms.html");
        profiler.exportChromeTraceFile("flamegraph_assignment04_push_constants_dynamic_uniforms.json");
        profiler.cleanupGpu();


        // STEP 13: Cleanup Resources
        vkUnmapMemory(device, dynamicMaterialBufferMemory);
        vkDestroyBuffer(device, dynamicMaterialBuffer, nullptr);
        vkFreeMemory(device, dynamicMaterialBufferMemory, nullptr);

        vkUnmapMemory(device, sceneUBOBufferMemory);
        vkDestroyBuffer(device, sceneUBOBuffer, nullptr);
        vkFreeMemory(device, sceneUBOBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

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

    std::cout << "Assignment 4 finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
