// ============================================================================
// Assignment 16: GPU-Driven Scene Culling & Multi-Draw Indirect Count
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Vulkan 1.4 Core vkCmdDrawIndexedIndirectCount (drawIndirectCount feature)
//   - GPU-side compute frustum culling & dynamic command stream generation
//   - Frustum plane extraction from Camera View-Projection matrix
//   - Dynamic CountBuffer atomic resets & increments on GPU
//   - Vulkan 1.4 Core Synchronization2 memory barriers (Compute Write -> Indirect/Index/Vertex Reads)
//   - Dynamic Rendering with Depth Testing and Smooth Camera Animation
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
#include <random>
#include <cstring>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Data Structures matching GLSL Shaders (cull.comp, scene.vert, scene.frag)
// ----------------------------------------------------------------------------
struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 normal;
    float texCoord[2];

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDesc;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attrDescs{};

        // Position (location = 0)
        attrDescs[0].binding = 0;
        attrDescs[0].location = 0;
        attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrDescs[0].offset = offsetof(Vertex, pos);

        // Normal (location = 1)
        attrDescs[1].binding = 0;
        attrDescs[1].location = 1;
        attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrDescs[1].offset = offsetof(Vertex, normal);

        // UV (location = 2)
        attrDescs[2].binding = 0;
        attrDescs[2].location = 2;
        attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
        attrDescs[2].offset = offsetof(Vertex, texCoord);

        return attrDescs;
    }
};

struct ObjectData {
    float sphereBounds[4]; // xyz: center, w: radius
    vk_math::Mat4 modelMatrix;
};

struct DrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
};

struct CullingPushConstants {
    float frustumPlanes[6][4];
    uint32_t totalObjects;
    uint32_t indexCountPerObject;
};

struct SceneCameraPushConstants {
    vk_math::Mat4 viewProj;
};

// ----------------------------------------------------------------------------
// Geometry Generation (Unit Cube with 24 vertices and 36 indices)
// ----------------------------------------------------------------------------
void generateCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    vertices = {
        // Front face (Z = 0.5)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

        // Back face (Z = -0.5)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

        // Top face (Y = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},

        // Bottom face (Y = 0.5)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

        // Right face (X = 0.5)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

        // Left face (X = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
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
// Helper: Memory Type Filter
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Helper: Create GPU Buffer
// ----------------------------------------------------------------------------
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

static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
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

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// ----------------------------------------------------------------------------
// Helper: Depth Image Creation
// ----------------------------------------------------------------------------
struct DepthImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

DepthImage createDepthImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height) {
    DepthImage depth{};
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &depth.image), "Failed to create depth image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, depth.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &depth.memory), "Failed to allocate depth image memory");
    vk_common::check_vk_result(vkBindImageMemory(device, depth.image, depth.memory, 0), "Failed to bind depth image memory");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depth.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &depth.view), "Failed to create depth image view");

    return depth;
}

// ----------------------------------------------------------------------------
// Extract 6 Frustum Planes from View-Projection Matrix (Column-Major)
// ----------------------------------------------------------------------------
void extractFrustumPlanes(const vk_math::Mat4& vp, float planes[6][4]) {
    // Mat4 column-major: m[col][row], row i is (m[0][i], m[1][i], m[2][i], m[3][i])
    // Left: row3 + row0
    planes[0][0] = vp.m[0][3] + vp.m[0][0];
    planes[0][1] = vp.m[1][3] + vp.m[1][0];
    planes[0][2] = vp.m[2][3] + vp.m[2][0];
    planes[0][3] = vp.m[3][3] + vp.m[3][0];

    // Right: row3 - row0
    planes[1][0] = vp.m[0][3] - vp.m[0][0];
    planes[1][1] = vp.m[1][3] - vp.m[1][0];
    planes[1][2] = vp.m[2][3] - vp.m[2][0];
    planes[1][3] = vp.m[3][3] - vp.m[3][0];

    // Bottom: row3 + row1
    planes[2][0] = vp.m[0][3] + vp.m[0][1];
    planes[2][1] = vp.m[1][3] + vp.m[1][1];
    planes[2][2] = vp.m[2][3] + vp.m[2][1];
    planes[2][3] = vp.m[3][3] + vp.m[3][1];

    // Top: row3 - row1
    planes[3][0] = vp.m[0][3] - vp.m[0][1];
    planes[3][1] = vp.m[1][3] - vp.m[1][1];
    planes[3][2] = vp.m[2][3] - vp.m[2][1];
    planes[3][3] = vp.m[3][3] - vp.m[3][1];

    // Near: row2 (Vulkan depth [0, 1])
    planes[4][0] = vp.m[0][2];
    planes[4][1] = vp.m[1][2];
    planes[4][2] = vp.m[2][2];
    planes[4][3] = vp.m[3][2];

    // Far: row3 - row2
    planes[5][0] = vp.m[0][3] - vp.m[0][2];
    planes[5][1] = vp.m[1][3] - vp.m[1][2];
    planes[5][2] = vp.m[2][3] - vp.m[2][2];
    planes[5][3] = vp.m[3][3] - vp.m[3][2];

    // Normalize each plane
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(planes[i][0] * planes[i][0] + planes[i][1] * planes[i][1] + planes[i][2] * planes[i][2]);
        if (len > 0.0f) {
            planes[i][0] /= len;
            planes[i][1] /= len;
            planes[i][2] /= len;
            planes[i][3] /= len;
        }
    }
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 16: GPU-Driven Draw Indirect Count (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Vulkan 1.4 Core vkCmdDrawIndexedIndirectCount,\n";
    std::cout << "           GPU Compute Frustum Culling, Dynamic CountBuffer,\n";
    std::cout << "           Compute-to-Draw Synchronization2 & Dynamic Rendering\n";
    std::cout << "========================================================\n";

    constexpr uint32_t WIDTH = 1280;
    constexpr uint32_t HEIGHT = 720;
    constexpr uint32_t TOTAL_OBJECTS = 4096; // 16 x 16 x 16 3D grid of objects

    try {
        // STEP 1: Window & Instance Creation
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 16: GPU-Driven Draw Indirect Count (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        // STEP 2: Device Creation with drawIndirectCount & multiDrawIndirect enabled
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // Vulkan 1.2 Core Features (drawIndirectCount)
        VkPhysicalDeviceVulkan12Features vulkan12Features{};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.drawIndirectCount = VK_TRUE;
        vulkan12Features.bufferDeviceAddress = VK_TRUE;

        // Vulkan 1.3 Core Features (Dynamic Rendering & Synchronization2)
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &vulkan12Features;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;
        vulkan13Features.maintenance4 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;
        deviceFeatures2.features.multiDrawIndirect = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &deviceFeatures2;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device = VK_NULL_HANDLE;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create Vulkan 1.4 Logical Device");

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // Load vkCmdDrawIndexedIndirectCount function pointer
        PFN_vkCmdDrawIndexedIndirectCount pfnCmdDrawIndexedIndirectCount = 
            (PFN_vkCmdDrawIndexedIndirectCount)vkGetDeviceProcAddr(device, "vkCmdDrawIndexedIndirectCount");
        if (!pfnCmdDrawIndexedIndirectCount) {
            pfnCmdDrawIndexedIndirectCount = (PFN_vkCmdDrawIndexedIndirectCount)vkGetDeviceProcAddr(device, "vkCmdDrawIndexedIndirectCountKHR");
        }
        if (!pfnCmdDrawIndexedIndirectCount) {
            throw std::runtime_error("vkCmdDrawIndexedIndirectCount function pointer not available!");
        }

        std::cout << "[Device] Vulkan 1.4 Device created with drawIndirectCount & Synchronization2 support.\n";

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
        DepthImage depthAttachment = createDepthImage(device, physicalDevice, WIDTH, HEIGHT);

        // STEP 5: Command Pool Setup
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 6: Generate Cube Mesh Geometry
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        generateCube(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize + indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize + indexBufferSize, 0, &data);
        std::memcpy(data, vertices.data(), (size_t)vertexBufferSize);
        std::memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        VkBuffer vertexBuffer, indexBuffer;
        VkDeviceMemory vertexBufferMemory, indexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     vertexBuffer, vertexBufferMemory);
        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, vertexBufferSize);

        // Copy indices
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);
        VkCommandBufferBeginInfo beginInfoCopy{};
        beginInfoCopy.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfoCopy.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfoCopy);
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = vertexBufferSize;
        copyRegion.dstOffset = 0;
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer, indexBuffer, 1, &copyRegion);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitCopy{};
        submitCopy.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitCopy.commandBufferCount = 1;
        submitCopy.pCommandBuffers = &cmd;
        vkQueueSubmit(graphicsQueue, 1, &submitCopy, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        // STEP 7: Generate Object Data (4096 objects distributed in 3D space)
        std::vector<ObjectData> objects(TOTAL_OBJECTS);
        const int dim = 16; // 16x16x16 = 4096
        const float spacing = 3.5f;
        const float offset = (dim - 1) * spacing * 0.5f;

        for (int z = 0; z < dim; ++z) {
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    uint32_t idx = z * dim * dim + y * dim + x;
                    float px = x * spacing - offset;
                    float py = y * spacing - offset;
                    float pz = z * spacing - offset;

                    // Sphere bounding volume: center = (px, py, pz), radius = 0.866 (half-diagonal of unit cube is sqrt(3)*0.5)
                    objects[idx].sphereBounds[0] = px;
                    objects[idx].sphereBounds[1] = py;
                    objects[idx].sphereBounds[2] = pz;
                    objects[idx].sphereBounds[3] = 0.87f; // Bounding radius

                    objects[idx].modelMatrix = vk_math::Mat4::translate(vk_math::Vec3(px, py, pz));
                }
            }
        }

        VkDeviceSize objectBufferSize = sizeof(ObjectData) * objects.size();
        VkBuffer objectBuffer;
        VkDeviceMemory objectBufferMemory;
        createBuffer(device, physicalDevice, objectBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     objectBuffer, objectBufferMemory);

        // Upload Object Data via Staging Buffer
        VkBuffer objStagingBuffer;
        VkDeviceMemory objStagingMemory;
        createBuffer(device, physicalDevice, objectBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     objStagingBuffer, objStagingMemory);

        void* objDataPtr;
        vkMapMemory(device, objStagingMemory, 0, objectBufferSize, 0, &objDataPtr);
        std::memcpy(objDataPtr, objects.data(), (size_t)objectBufferSize);
        vkUnmapMemory(device, objStagingMemory);

        copyBuffer(device, commandPool, graphicsQueue, objStagingBuffer, objectBuffer, objectBufferSize);
        vkDestroyBuffer(device, objStagingBuffer, nullptr);
        vkFreeMemory(device, objStagingMemory, nullptr);

        // STEP 8: Create Indirect Draw Buffer & Count Buffer
        VkDeviceSize indirectBufferSize = sizeof(DrawIndexedIndirectCommand) * TOTAL_OBJECTS;
        VkDeviceSize countBufferSize = sizeof(uint32_t);

        VkBuffer indirectBuffer, countBuffer;
        VkDeviceMemory indirectBufferMemory, countBufferMemory;

        // Indirect buffer is written by compute shader and read by draw indirect command
        createBuffer(device, physicalDevice, indirectBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     indirectBuffer, indirectBufferMemory);

        // Count buffer is written by compute atomicAdd and read by draw indirect count command & transfer
        createBuffer(device, physicalDevice, countBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     countBuffer, countBufferMemory);

        // Host Readback Buffer for reading culled draw count on CPU
        VkBuffer countReadbackBuffer;
        VkDeviceMemory countReadbackMemory;
        createBuffer(device, physicalDevice, countBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     countReadbackBuffer, countReadbackMemory);

        // STEP 9: Descriptor Set Layout & Pipeline Layout Setup
        // Compute Descriptor Set Layout: Binding 0 (ObjectBuffer), Binding 1 (IndirectDrawBuffer), Binding 2 (CountBuffer)
        std::array<VkDescriptorSetLayoutBinding, 3> compBindings{};
        compBindings[0].binding = 0;
        compBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compBindings[0].descriptorCount = 1;
        compBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        compBindings[1].binding = 1;
        compBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compBindings[1].descriptorCount = 1;
        compBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        compBindings[2].binding = 2;
        compBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compBindings[2].descriptorCount = 1;
        compBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo compLayoutInfo{};
        compLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        compLayoutInfo.bindingCount = static_cast<uint32_t>(compBindings.size());
        compLayoutInfo.pBindings = compBindings.data();

        VkDescriptorSetLayout computeDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &compLayoutInfo, nullptr, &computeDescriptorSetLayout), "Failed to create compute descriptor layout");

        // Graphics Descriptor Set Layout: Binding 0 (ObjectBuffer for gl_BaseInstance model transform fetch)
        VkDescriptorSetLayoutBinding gfxBinding{};
        gfxBinding.binding = 0;
        gfxBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        gfxBinding.descriptorCount = 1;
        gfxBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo gfxLayoutInfo{};
        gfxLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        gfxLayoutInfo.bindingCount = 1;
        gfxLayoutInfo.pBindings = &gfxBinding;

        VkDescriptorSetLayout graphicsDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &gfxLayoutInfo, nullptr, &graphicsDescriptorSetLayout), "Failed to create graphics descriptor layout");

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 4; // 3 for compute + 1 for graphics

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();
        descPoolInfo.maxSets = 2;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        // Allocate Compute Descriptor Set
        VkDescriptorSetAllocateInfo compAllocInfo{};
        compAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        compAllocInfo.descriptorPool = descriptorPool;
        compAllocInfo.descriptorSetCount = 1;
        compAllocInfo.pSetLayouts = &computeDescriptorSetLayout;

        VkDescriptorSet computeDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &compAllocInfo, &computeDescriptorSet), "Failed to allocate compute descriptor set");

        // Allocate Graphics Descriptor Set
        VkDescriptorSetAllocateInfo gfxAllocInfo{};
        gfxAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        gfxAllocInfo.descriptorPool = descriptorPool;
        gfxAllocInfo.descriptorSetCount = 1;
        gfxAllocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        VkDescriptorSet graphicsDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &gfxAllocInfo, &graphicsDescriptorSet), "Failed to allocate graphics descriptor set");

        // Update Compute Descriptor Set
        VkDescriptorBufferInfo objBufInfo{objectBuffer, 0, objectBufferSize};
        VkDescriptorBufferInfo indBufInfo{indirectBuffer, 0, indirectBufferSize};
        VkDescriptorBufferInfo cntBufInfo{countBuffer, 0, countBufferSize};

        std::array<VkWriteDescriptorSet, 3> compWrites{};
        compWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[0].dstSet = computeDescriptorSet;
        compWrites[0].dstBinding = 0;
        compWrites[0].descriptorCount = 1;
        compWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compWrites[0].pBufferInfo = &objBufInfo;

        compWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[1].dstSet = computeDescriptorSet;
        compWrites[1].dstBinding = 1;
        compWrites[1].descriptorCount = 1;
        compWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compWrites[1].pBufferInfo = &indBufInfo;

        compWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        compWrites[2].dstSet = computeDescriptorSet;
        compWrites[2].dstBinding = 2;
        compWrites[2].descriptorCount = 1;
        compWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        compWrites[2].pBufferInfo = &cntBufInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(compWrites.size()), compWrites.data(), 0, nullptr);

        // Update Graphics Descriptor Set
        VkWriteDescriptorSet gfxWrite{};
        gfxWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gfxWrite.dstSet = graphicsDescriptorSet;
        gfxWrite.dstBinding = 0;
        gfxWrite.descriptorCount = 1;
        gfxWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        gfxWrite.pBufferInfo = &objBufInfo;

        vkUpdateDescriptorSets(device, 1, &gfxWrite, 0, nullptr);

        // Compute Pipeline Layout with Push Constants
        VkPushConstantRange compPushRange{};
        compPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        compPushRange.offset = 0;
        compPushRange.size = sizeof(CullingPushConstants);

        VkPipelineLayoutCreateInfo compPipelineLayoutInfo{};
        compPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        compPipelineLayoutInfo.setLayoutCount = 1;
        compPipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        compPipelineLayoutInfo.pushConstantRangeCount = 1;
        compPipelineLayoutInfo.pPushConstantRanges = &compPushRange;

        VkPipelineLayout computePipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &compPipelineLayoutInfo, nullptr, &computePipelineLayout), "Failed to create compute pipeline layout");

        // Graphics Pipeline Layout with Push Constants
        VkPushConstantRange gfxPushRange{};
        gfxPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        gfxPushRange.offset = 0;
        gfxPushRange.size = sizeof(SceneCameraPushConstants);

        VkPipelineLayoutCreateInfo gfxPipelineLayoutInfo{};
        gfxPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        gfxPipelineLayoutInfo.setLayoutCount = 1;
        gfxPipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;
        gfxPipelineLayoutInfo.pushConstantRangeCount = 1;
        gfxPipelineLayoutInfo.pPushConstantRanges = &gfxPushRange;

        VkPipelineLayout graphicsPipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &gfxPipelineLayoutInfo, nullptr, &graphicsPipelineLayout), "Failed to create graphics pipeline layout");

        // Load SPIR-V Shaders
        std::string compPath = "shaders/cull.comp.spv";
        std::string vertPath = "shaders/scene.vert.spv";
        std::string fragPath = "shaders/scene.frag.spv";
        if (!std::filesystem::exists(compPath)) compPath = "assignment16_gpu_driven_draw_indirect_count/shaders/cull.comp.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment16_gpu_driven_draw_indirect_count/shaders/scene.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment16_gpu_driven_draw_indirect_count/shaders/scene.frag.spv";

        auto compCode = vulkan_utils::readFile(compPath);
        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

        VkShaderModule compModule = vulkan_utils::createShaderModule(device, compCode);
        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        // STEP 10: Create Compute Pipeline
        VkComputePipelineCreateInfo compPipelineInfo{};
        compPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compPipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compModule, "main", nullptr};
        compPipelineInfo.layout = computePipelineLayout;

        VkPipeline computePipeline;
        vk_common::check_vk_result(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compPipelineInfo, nullptr, &computePipeline), "Failed to create compute pipeline");

        // STEP 11: Create Graphics Pipeline with Dynamic Rendering & Depth Testing
        VkPipelineShaderStageCreateInfo graphicsShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
        };

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

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = surfaceFormat.format;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
        pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

        VkGraphicsPipelineCreateInfo graphicsPipelineInfo{};
        graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineInfo.pNext = &pipelineRenderingCreateInfo;
        graphicsPipelineInfo.stageCount = 2;
        graphicsPipelineInfo.pStages = graphicsShaderStages;
        graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
        graphicsPipelineInfo.pInputAssemblyState = &inputAssembly;
        graphicsPipelineInfo.pViewportState = &viewportState;
        graphicsPipelineInfo.pRasterizationState = &rasterizer;
        graphicsPipelineInfo.pMultisampleState = &multisampling;
        graphicsPipelineInfo.pDepthStencilState = &depthStencil;
        graphicsPipelineInfo.pColorBlendState = &colorBlending;
        graphicsPipelineInfo.pDynamicState = &dynamicState;
        graphicsPipelineInfo.layout = graphicsPipelineLayout;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 12: Command Buffer & Sync Primitives
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo frameCmdAllocInfo{};
        frameCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        frameCmdAllocInfo.commandPool = commandPool;
        frameCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        frameCmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &frameCmdAllocInfo, &commandBuffer);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

        std::cout << "[Pipeline] GPU-Driven Culling & Multi-Draw Indirect Count ready. Total Scene Objects: " << TOTAL_OBJECTS << "\n";
        std::cout << "Starting Simulation & Rendering Loop...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameIndex = 0;

        // STEP 13: Render Loop
        while (!glfwWindowShouldClose(window)) {
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

            // Dynamic Camera Animation (Orbiting Around Scene)
            float camRadius = 45.0f + std::sin(time * 0.3f) * 10.0f;
            float camAngle = time * 0.4f;
            vk_math::Vec3 eyePos(std::cos(camAngle) * camRadius, 18.0f + std::sin(time * 0.5f) * 12.0f, std::sin(camAngle) * camRadius);
            vk_math::Vec3 targetPos(0.0f, 0.0f, 0.0f);

            vk_math::Mat4 view = vk_math::Mat4::lookAt(eyePos, targetPos, vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(
                vk_math::radians(60.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.5f,
                200.0f
            );
            vk_math::Mat4 viewProj = proj * view;

            // Extract Frustum Planes for Compute Culling
            CullingPushConstants cullingPush{};
            extractFrustumPlanes(viewProj, cullingPush.frustumPlanes);
            cullingPush.totalObjects = TOTAL_OBJECTS;
            cullingPush.indexCountPerObject = static_cast<uint32_t>(indices.size());

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo);

            // 1. Reset Count Buffer to 0 on GPU
            vkCmdFillBuffer(commandBuffer, countBuffer, 0, sizeof(uint32_t), 0);

            // Barrier: Transfer (FillBuffer) -> Compute Shader Write (atomicAdd)
            VkBufferMemoryBarrier2 fillToComputeBarrier{};
            fillToComputeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            fillToComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            fillToComputeBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            fillToComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            fillToComputeBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            fillToComputeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fillToComputeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fillToComputeBarrier.buffer = countBuffer;
            fillToComputeBarrier.offset = 0;
            fillToComputeBarrier.size = sizeof(uint32_t);

            VkDependencyInfo fillDepInfo{};
            fillDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            fillDepInfo.bufferMemoryBarrierCount = 1;
            fillDepInfo.pBufferMemoryBarriers = &fillToComputeBarrier;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &fillDepInfo);

            // 2. Dispatch Frustum Culling Compute Pass
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingPushConstants), &cullingPush);

            uint32_t groupCountX = (TOTAL_OBJECTS + 63) / 64; // local_size_x = 64
            vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

            // 3. Synchronization2: Compute Write -> Draw Indirect & Transfer Read Barriers
            std::array<VkBufferMemoryBarrier2, 2> compToDrawBarriers{};
            // Indirect Command Buffer: Compute Write -> Indirect Command Read
            compToDrawBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            compToDrawBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            compToDrawBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            compToDrawBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            compToDrawBarriers[0].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            compToDrawBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            compToDrawBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            compToDrawBarriers[0].buffer = indirectBuffer;
            compToDrawBarriers[0].offset = 0;
            compToDrawBarriers[0].size = indirectBufferSize;

            // Count Buffer: Compute Write -> Indirect Command Read + Transfer Read
            compToDrawBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            compToDrawBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            compToDrawBarriers[1].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            compToDrawBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            compToDrawBarriers[1].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT;
            compToDrawBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            compToDrawBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            compToDrawBarriers[1].buffer = countBuffer;
            compToDrawBarriers[1].offset = 0;
            compToDrawBarriers[1].size = countBufferSize;

            VkDependencyInfo compDepInfo{};
            compDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            compDepInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(compToDrawBarriers.size());
            compDepInfo.pBufferMemoryBarriers = compToDrawBarriers.data();
            vk14.vkCmdPipelineBarrier2(commandBuffer, &compDepInfo);

            // Copy draw count to readback buffer for telemetry
            VkBufferCopy countCopyRegion{0, 0, sizeof(uint32_t)};
            vkCmdCopyBuffer(commandBuffer, countBuffer, countReadbackBuffer, 1, &countCopyRegion);

            // 4. Image Transitions for Dynamic Rendering
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

            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                depthAttachment.image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );

            // 5. Dynamic Rendering Pass
            VkRenderingAttachmentInfo colorAttInfo{};
            colorAttInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttInfo.imageView = swapchainImageViews[imageIndex];
            colorAttInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttInfo.clearValue.color = {{0.03f, 0.04f, 0.08f, 1.0f}}; // Dark slate background

            VkRenderingAttachmentInfo depthAttInfo{};
            depthAttInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttInfo.imageView = depthAttachment.view;
            depthAttInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttInfo.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttInfo;
            renderingInfo.pDepthAttachment = &depthAttInfo;

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Bind Vertex & Index Buffers
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Bind ObjectBuffer Descriptor Set for gl_BaseInstance lookup
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsDescriptorSet, 0, nullptr);

            // Push Camera ViewProj Matrix
            SceneCameraPushConstants camPush{viewProj};
            vkCmdPushConstants(commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SceneCameraPushConstants), &camPush);

            // 6. Vulkan 1.4 Core Multi-Draw Indirect Count Call
            pfnCmdDrawIndexedIndirectCount(
                commandBuffer,
                indirectBuffer,                     // VkBuffer containing VkDrawIndexedIndirectCommand structs
                0,                                  // offset in bytes
                countBuffer,                        // VkBuffer containing uint32_t draw count
                0,                                  // countBufferOffset in bytes
                TOTAL_OBJECTS,                      // maxDrawCount
                sizeof(DrawIndexedIndirectCommand)  // stride
            );

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
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

            // Present Swapchain Image
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);

            // Telemetry: Read back culled count periodically
            if (frameIndex % 60 == 0) {
                vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
                void* readbackPtr;
                vkMapMemory(device, countReadbackMemory, 0, sizeof(uint32_t), 0, &readbackPtr);
                uint32_t visibleCount = *static_cast<uint32_t*>(readbackPtr);
                vkUnmapMemory(device, countReadbackMemory);

                float cullRatio = (1.0f - static_cast<float>(visibleCount) / static_cast<float>(TOTAL_OBJECTS)) * 100.0f;
                std::cout << "[GPU Frustum Culling | Frame #" << frameIndex << "] Visible Objects: " 
                          << visibleCount << " / " << TOTAL_OBJECTS 
                          << " (" << cullRatio << "% culled by GPU compute)" << std::endl;
            }

            frameIndex++;
        }

        vkDeviceWaitIdle(device);

        // Cleanup
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);

        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, compModule, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);

        vkDestroyBuffer(device, countReadbackBuffer, nullptr);
        vkFreeMemory(device, countReadbackMemory, nullptr);
        vkDestroyBuffer(device, countBuffer, nullptr);
        vkFreeMemory(device, countBufferMemory, nullptr);
        vkDestroyBuffer(device, indirectBuffer, nullptr);
        vkFreeMemory(device, indirectBufferMemory, nullptr);
        vkDestroyBuffer(device, objectBuffer, nullptr);
        vkFreeMemory(device, objectBufferMemory, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyImageView(device, depthAttachment.view, nullptr);
        vkDestroyImage(device, depthAttachment.image, nullptr);
        vkFreeMemory(device, depthAttachment.memory, nullptr);

        for (auto view : swapchainImageViews) {
            vkDestroyImageView(device, view, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "Assignment 16 execution completed cleanly.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
