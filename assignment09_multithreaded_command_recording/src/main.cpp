// ============================================================================
// Assignment 9: Multi-Threaded Command Recording with Timeline Semaphores
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Thread-safe independent VkCommandPool per worker CPU thread
//   - Secondary Command Buffers recorded concurrently across threads
//     (`VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT` & `VkCommandBufferInheritanceRenderingInfo`)
//   - Primary Command Buffer aggregation via `vkCmdExecuteCommands`
//   - Vulkan 1.4 Timeline Semaphores (`VK_SEMAPHORE_TYPE_TIMELINE`, `VkSemaphoreTimelineWaitInfo`)
//   - CPU Host Query & Wait on Timeline Semaphore (`vkWaitSemaphores` / `vkGetSemaphoreCounterValue`)
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
#include <thread>
#include <future>
#include <cstring>
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
    float padding[2];
};

static size_t getAlignedSize(size_t size, size_t alignment) {
    if (alignment > 0) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
    return size;
}

// ----------------------------------------------------------------------------
// Geometry Generation (Cube)
// ----------------------------------------------------------------------------
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

void copyBuffer(
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

// ----------------------------------------------------------------------------
// Multi-Threading Context Structure
// ----------------------------------------------------------------------------
struct ThreadContext {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer secondaryCmdBuffer = VK_NULL_HANDLE;
    uint32_t threadIndex = 0;
    uint32_t startObjIndex = 0;
    uint32_t objCount = 0;
};

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 9: Multi-Threaded Recording (Vulkan 1.4 Standard)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: Thread-Safe Pools, Secondary Buffers, Timeline Semaphores" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        // STEP 1: Window, Vulkan 1.4 Instance, and Surface
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 9: Multi-Threaded Recording (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // STEP 2: Device with Timeline Semaphore & Dynamic Rendering enabled
        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        std::cout << "Vulkan 1.4 Logical Device initialized for Assignment 9." << std::endl;

        // STEP 3: Swapchain Setup
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

        // STEP 4: Depth Attachment Setup
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;

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

        VkMemoryRequirements depthMemRequirements;
        vkGetImageMemoryRequirements(device, depthImage, &depthMemRequirements);

        VkMemoryAllocateInfo depthAllocInfo{};
        depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAllocInfo.allocationSize = depthMemRequirements.size;
        depthAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, depthMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vk_common::check_vk_result(vkAllocateMemory(device, &depthAllocInfo, nullptr, &depthImageMemory), "Failed to allocate depth image memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth image memory");

        VkImageView depthImageView;
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

        // STEP 5: Main Command Pool (for staging and primary command buffer)
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool mainCommandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &mainCommandPool), "Failed to create main command pool");

        // STEP 6: Multi-Threading Worker Thread Command Pools & Contexts
        const uint32_t THREAD_COUNT = 4;
        const uint32_t TOTAL_OBJECTS = 16;
        const uint32_t OBJECTS_PER_THREAD = TOTAL_OBJECTS / THREAD_COUNT;

        std::vector<ThreadContext> threadContexts(THREAD_COUNT);
        for (uint32_t t = 0; t < THREAD_COUNT; ++t) {
            threadContexts[t].threadIndex = t;
            threadContexts[t].startObjIndex = t * OBJECTS_PER_THREAD;
            threadContexts[t].objCount = OBJECTS_PER_THREAD;

            VkCommandPoolCreateInfo threadPoolInfo{};
            threadPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            threadPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            threadPoolInfo.queueFamilyIndex = graphicsQueueFamily;

            vk_common::check_vk_result(vkCreateCommandPool(device, &threadPoolInfo, nullptr, &threadContexts[t].commandPool), "Failed to create thread command pool");

            VkCommandBufferAllocateInfo secAllocInfo{};
            secAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            secAllocInfo.commandPool = threadContexts[t].commandPool;
            secAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            secAllocInfo.commandBufferCount = 1;

            vk_common::check_vk_result(vkAllocateCommandBuffers(device, &secAllocInfo, &threadContexts[t].secondaryCmdBuffer), "Failed to allocate secondary command buffer");
        }

        // STEP 7: Mesh Geometry Setup
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

        copyBuffer(device, mainCommandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
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

        copyBuffer(device, mainCommandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 8: Uniform Buffers (Scene UBO & Dynamic Material UBO)
        VkPhysicalDeviceProperties devProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &devProperties);
        size_t minUboAlignment = devProperties.limits.minUniformBufferOffsetAlignment;
        size_t dynamicAlignment = getAlignedSize(sizeof(MaterialData), minUboAlignment);

        // 16 rich material definitions across the 4 worker threads
        std::vector<MaterialData> materials(TOTAL_OBJECTS);
        for (uint32_t i = 0; i < TOTAL_OBJECTS; ++i) {
            float hue = (float)i / (float)TOTAL_OBJECTS;
            float r = 0.5f + 0.5f * std::cos(6.28318f * (hue + 0.0f / 3.0f));
            float g = 0.5f + 0.5f * std::cos(6.28318f * (hue + 1.0f / 3.0f));
            float b = 0.5f + 0.5f * std::cos(6.28318f * (hue + 2.0f / 3.0f));

            materials[i].baseColor[0] = r;
            materials[i].baseColor[1] = g;
            materials[i].baseColor[2] = b;
            materials[i].baseColor[3] = 1.0f;

            materials[i].ambient[0] = 0.2f;
            materials[i].ambient[1] = 0.2f;
            materials[i].ambient[2] = 0.2f;
            materials[i].ambient[3] = 1.0f;

            materials[i].diffuse[0] = 0.8f;
            materials[i].diffuse[1] = 0.8f;
            materials[i].diffuse[2] = 0.8f;
            materials[i].diffuse[3] = 1.0f;

            materials[i].specularRoughness[0] = 1.0f;
            materials[i].specularRoughness[1] = 1.0f;
            materials[i].specularRoughness[2] = 1.0f;
            materials[i].specularRoughness[3] = 32.0f;
        }

        VkDeviceSize sceneUBOSize = sizeof(SceneUBO);
        VkBuffer sceneUBOBuffer;
        VkDeviceMemory sceneUBOBufferMemory;
        createBuffer(device, physicalDevice, sceneUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sceneUBOBuffer, sceneUBOBufferMemory);

        void* sceneUBOMapped = nullptr;
        vkMapMemory(device, sceneUBOBufferMemory, 0, sceneUBOSize, 0, &sceneUBOMapped);

        VkDeviceSize dynamicMaterialBufferSize = TOTAL_OBJECTS * dynamicAlignment;
        VkBuffer dynamicMaterialBuffer;
        VkDeviceMemory dynamicMaterialBufferMemory;
        createBuffer(device, physicalDevice, dynamicMaterialBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     dynamicMaterialBuffer, dynamicMaterialBufferMemory);

        void* dynamicMaterialMapped = nullptr;
        vkMapMemory(device, dynamicMaterialBufferMemory, 0, dynamicMaterialBufferSize, 0, &dynamicMaterialMapped);

        for (uint32_t i = 0; i < TOTAL_OBJECTS; ++i) {
            char* dest = static_cast<char*>(dynamicMaterialMapped) + (i * dynamicAlignment);
            std::memcpy(dest, &materials[i], sizeof(MaterialData));
        }

        // STEP 9: Descriptor Set Layout, Pool, and Allocation
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

        VkDescriptorSetAllocateInfo descAllocInfo{};
        descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descAllocInfo.descriptorPool = descriptorPool;
        descAllocInfo.descriptorSetCount = 1;
        descAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &descAllocInfo, &descriptorSet), "Failed to allocate descriptor set");

        VkDescriptorBufferInfo sceneBufferInfoDesc{};
        sceneBufferInfoDesc.buffer = sceneUBOBuffer;
        sceneBufferInfoDesc.offset = 0;
        sceneBufferInfoDesc.range = sizeof(SceneUBO);

        VkDescriptorBufferInfo dynamicBufferInfoDesc{};
        dynamicBufferInfoDesc.buffer = dynamicMaterialBuffer;
        dynamicBufferInfoDesc.offset = 0;
        dynamicBufferInfoDesc.range = sizeof(MaterialData);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &sceneBufferInfoDesc;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &dynamicBufferInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        // STEP 10: Pipeline Layout with Push Constants
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

        // STEP 11: Graphics Pipeline
        std::string vertPath = "shaders/object.vert.spv";
        std::string fragPath = "shaders/object.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment09_multithreaded_command_recording/shaders/object.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment09_multithreaded_command_recording/shaders/object.frag.spv";

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

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 12: Primary Command Buffer & Vulkan 1.4 Timeline Semaphore
        VkCommandBufferAllocateInfo mainCmdAllocInfo{};
        mainCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        mainCmdAllocInfo.commandPool = mainCommandPool;
        mainCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        mainCmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer primaryCommandBuffer;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &mainCmdAllocInfo, &primaryCommandBuffer), "Failed to allocate primary command buffer");

        // Binary Swapchain semaphores
        VkSemaphoreCreateInfo binarySemaphoreInfo{};
        binarySemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        vk_common::check_vk_result(vkCreateSemaphore(device, &binarySemaphoreInfo, nullptr, &imageAvailableSemaphore), "Failed to create image available semaphore");
        vk_common::check_vk_result(vkCreateSemaphore(device, &binarySemaphoreInfo, nullptr, &renderFinishedSemaphore), "Failed to create render finished semaphore");

        // Vulkan 1.4 Timeline Semaphore setup
        VkSemaphoreTypeCreateInfo timelineTypeInfo{};
        timelineTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineTypeInfo.initialValue = 0;

        VkSemaphoreCreateInfo timelineSemaphoreInfo{};
        timelineSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        timelineSemaphoreInfo.pNext = &timelineTypeInfo;

        VkSemaphore timelineSemaphore;
        vk_common::check_vk_result(vkCreateSemaphore(device, &timelineSemaphoreInfo, nullptr, &timelineSemaphore), "Failed to create timeline semaphore");

        std::cout << "Vulkan 1.4 Timeline Semaphore created with initial value 0." << std::endl;
        std::cout << "Multi-Threaded Recording Pipeline configured with " << THREAD_COUNT << " worker CPU threads (" << TOTAL_OBJECTS << " total objects)." << std::endl;
        std::cout << "Starting multi-threaded rendering loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t currentTimelineValue = 0;

        // STEP 13: Multi-Threaded Render Loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // Acquire next image
            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // (1) Update Static Camera UBO
            SceneUBO sceneUBO{};
            sceneUBO.view = vk_math::Mat4::lookAt(
                vk_math::Vec3(0.0f, 6.0f, 12.0f),
                vk_math::Vec3(0.0f, 0.0f, 0.0f),
                vk_math::Vec3(0.0f, 1.0f, 0.0f)
            );
            sceneUBO.proj = vk_math::Mat4::perspective(
                vk_math::radians(45.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.1f,
                100.0f
            );
            std::memcpy(sceneUBOMapped, &sceneUBO, sizeof(sceneUBO));

            // (2) Dispatch Multi-Threaded Secondary Command Buffer Recording
            // Each CPU thread concurrently records its batch of objects into its own secondary command buffer
            std::vector<std::future<void>> workerFutures;
            for (uint32_t t = 0; t < THREAD_COUNT; ++t) {
                workerFutures.push_back(std::async(std::launch::async, [&, t]() {
                    ThreadContext& ctx = threadContexts[t];

                    // Reset thread-local command pool (thread safe)
                    vkResetCommandPool(device, ctx.commandPool, 0);

                    // Setup inheritance info for Vulkan 1.4 Dynamic Rendering
                    VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
                    inheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
                    inheritanceRenderingInfo.flags = 0;
                    inheritanceRenderingInfo.colorAttachmentCount = 1;
                    inheritanceRenderingInfo.pColorAttachmentFormats = &surfaceFormat.format;
                    inheritanceRenderingInfo.depthAttachmentFormat = depthFormat;
                    inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

                    VkCommandBufferInheritanceInfo inheritanceInfo{};
                    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
                    inheritanceInfo.pNext = &inheritanceRenderingInfo;
                    inheritanceInfo.renderPass = VK_NULL_HANDLE;
                    inheritanceInfo.subpass = 0;
                    inheritanceInfo.framebuffer = VK_NULL_HANDLE;

                    VkCommandBufferBeginInfo secBeginInfo{};
                    secBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    secBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
                    secBeginInfo.pInheritanceInfo = &inheritanceInfo;

                    vkBeginCommandBuffer(ctx.secondaryCmdBuffer, &secBeginInfo);

                    vkCmdBindPipeline(ctx.secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

                    VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
                    VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
                    vkCmdSetViewport(ctx.secondaryCmdBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(ctx.secondaryCmdBuffer, 0, 1, &scissor);

                    VkBuffer vertexBuffers[] = {vertexBuffer};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(ctx.secondaryCmdBuffer, 0, 1, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(ctx.secondaryCmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

                    // Record draw calls for this thread's partition
                    for (uint32_t i = 0; i < ctx.objCount; ++i) {
                        uint32_t objIdx = ctx.startObjIndex + i;

                        // Orbital dynamic grid arrangement
                        float angleOffset = (float)objIdx * (2.0f * vk_math::PI / (float)TOTAL_OBJECTS);
                        float radius = 3.5f + (float)(objIdx % 2) * 1.5f;
                        float currentAngle = time * (0.6f + 0.05f * (float)objIdx) + angleOffset;

                        float posX = std::cos(currentAngle) * radius;
                        float posZ = std::sin(currentAngle) * radius;
                        float posY = std::sin(time * 2.0f + (float)objIdx) * 1.0f;

                        vk_math::Mat4 model = vk_math::Mat4::translate(vk_math::Vec3(posX, posY, posZ))
                                            * vk_math::Mat4::rotate(time * vk_math::radians(60.0f + 10.0f * objIdx), vk_math::Vec3(0.5f, 1.0f, 0.2f))
                                            * vk_math::Mat4::scale(vk_math::Vec3(0.65f, 0.65f, 0.65f));

                        PushConstantData pc{};
                        pc.model = model;
                        pc.time = time;
                        pc.objectIndex = objIdx;

                        vkCmdPushConstants(
                            ctx.secondaryCmdBuffer,
                            pipelineLayout,
                            VK_SHADER_STAGE_VERTEX_BIT,
                            0,
                            sizeof(PushConstantData),
                            &pc
                        );

                        uint32_t dynamicOffset = static_cast<uint32_t>(objIdx * dynamicAlignment);
                        vkCmdBindDescriptorSets(
                            ctx.secondaryCmdBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &descriptorSet,
                            1,
                            &dynamicOffset
                        );

                        vkCmdDrawIndexed(ctx.secondaryCmdBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
                    }

                    vkEndCommandBuffer(ctx.secondaryCmdBuffer);
                }));
            }

            // Wait for all worker CPU threads to finish recording secondary command buffers
            for (auto& fut : workerFutures) {
                fut.get();
            }

            // (3) Record Primary Command Buffer
            vkResetCommandBuffer(primaryCommandBuffer, 0);
            VkCommandBufferBeginInfo primaryBeginInfo{};
            primaryBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(primaryCommandBuffer, &primaryBeginInfo);

            // Transition Swapchain Image
            vulkan_utils::pipelineBarrier2ImageTransition(
                primaryCommandBuffer,
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

            // Transition Depth Image
            vulkan_utils::pipelineBarrier2ImageTransition(
                primaryCommandBuffer,
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
            colorAttachment.clearValue.color = {{0.03f, 0.04f, 0.08f, 1.0f}};

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(primaryCommandBuffer, &renderingInfo);

            // Execute all secondary command buffers recorded by worker threads
            std::vector<VkCommandBuffer> secondaryCmdBuffers(THREAD_COUNT);
            for (uint32_t t = 0; t < THREAD_COUNT; ++t) {
                secondaryCmdBuffers[t] = threadContexts[t].secondaryCmdBuffer;
            }
            vkCmdExecuteCommands(primaryCommandBuffer, THREAD_COUNT, secondaryCmdBuffers.data());

            vk14.vkCmdEndRendering(primaryCommandBuffer);

            // Transition Swapchain Image for Presentation
            vulkan_utils::pipelineBarrier2ImageTransition(
                primaryCommandBuffer,
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

            vkEndCommandBuffer(primaryCommandBuffer);

            // (4) Submit with Vulkan 1.4 Timeline Semaphore Signaling
            currentTimelineValue++;

            // Binary semaphore for presentation
            std::array<VkSemaphore, 2> signalSemaphores = {renderFinishedSemaphore, timelineSemaphore};
            std::array<uint64_t, 2> signalValues = {0, currentTimelineValue};

            VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
            timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineSubmitInfo.signalSemaphoreValueCount = static_cast<uint32_t>(signalValues.size());
            timelineSubmitInfo.pSignalSemaphoreValues = signalValues.data();

            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = &timelineSubmitInfo;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &primaryCommandBuffer;
            submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
            submitInfo.pSignalSemaphores = signalSemaphores.data();

            vk_common::check_vk_result(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit primary command buffer");

            // (5) Host Wait on Timeline Semaphore (Validating Vulkan 1.4 Timeline Semaphore Synchronization)
            VkSemaphoreWaitInfo waitInfo{};
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &timelineSemaphore;
            waitInfo.pValues = &currentTimelineValue;

            vk_common::check_vk_result(vkWaitSemaphores(device, &waitInfo, UINT64_MAX), "Failed to wait on timeline semaphore on host");

            // Query Timeline Counter value to confirm monotonic progression
            uint64_t counterValue = 0;
            vk_common::check_vk_result(vkGetSemaphoreCounterValue(device, timelineSemaphore, &counterValue), "Failed to get timeline semaphore counter value");

            if (currentTimelineValue % 60 == 0) {
                std::cout << "[Timeline Semaphore] Frame #" << currentTimelineValue 
                          << " completed. Monotonic GPU counter value: " << counterValue << std::endl;
            }

            // Present Image
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }

        vkDeviceWaitIdle(device);

        // STEP 14: Cleanup Resources
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

        vkDestroySemaphore(device, timelineSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

        for (uint32_t t = 0; t < THREAD_COUNT; ++t) {
            vkDestroyCommandPool(device, threadContexts[t].commandPool, nullptr);
        }
        vkDestroyCommandPool(device, mainCommandPool, nullptr);

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

    std::cout << "Assignment 9 finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
