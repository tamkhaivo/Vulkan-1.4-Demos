// ============================================================================
// Assignment 8: Deferred Shading with Multiple Render Targets (MRT)
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Vulkan 1.4 Core Dynamic Rendering Local Reads (`dynamicRenderingLocalRead` feature)
//   - Multiple Render Targets (MRT): G-Buffer (Position, Normal, Albedo, Depth)
//   - Attachment format configurations:
//       * Position: VK_FORMAT_R16G16B16A16_SFLOAT
//       * Normal:   VK_FORMAT_R16G16B16A16_SFLOAT
//       * Albedo:   VK_FORMAT_R8G8B8A8_UNORM
//       * Depth:    VK_FORMAT_D32_SFLOAT
//   - Subpass input attachments sampled in fragment shader via `subpassLoad()`
//   - Zero legacy VkRenderPass and zero legacy VkFramebuffer objects
//   - Vulkan 1.4 Synchronization2 image transitions and BY_REGION local read hazards
//   - Multi-Point Light accumulation deferred shading with smooth attenuation and ACES tone mapping
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
    float cameraPos[4];
};

struct PointLight {
    float position[4]; // xyz = pos, w = radius
    float color[4];    // rgb = color, w = intensity
};

struct LightUBO {
    float viewPos[4];
    PointLight lights[6];
    int32_t lightCount;
    int32_t displayMode; // 0 = Full Deferred Lighting, 1 = Position, 2 = Normal, 3 = Albedo
    float padding[2];
};

// ----------------------------------------------------------------------------
// Geometry Generation: Torus, Cube, and Ground Plane
// ----------------------------------------------------------------------------
void generateSceneGeometry(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    vertices.clear();
    indices.clear();

    // Helper lambda to add a quad
    auto addQuad = [&](const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
        indices.push_back(baseIdx + 0);
    };

    // 1. Ground Plane (Y = -1.0) with subtle grid-colored tiles
    float planeSize = 6.0f;
    float planeY = -1.0f;
    int gridDivs = 8;
    float step = (planeSize * 2.0f) / gridDivs;

    for (int gz = 0; gz < gridDivs; ++gz) {
        for (int gx = 0; gx < gridDivs; ++gx) {
            float x0 = -planeSize + gx * step;
            float x1 = x0 + step;
            float z0 = -planeSize + gz * step;
            float z1 = z0 + step;

            vk_math::Vec3 col = ((gx + gz) % 2 == 0) ? vk_math::Vec3(0.35f, 0.38f, 0.45f) : vk_math::Vec3(0.22f, 0.25f, 0.32f);
            vk_math::Vec3 norm(0.0f, 1.0f, 0.0f);

            Vertex v0{{x0, planeY, z0}, col, norm};
            Vertex v1{{x1, planeY, z0}, col, norm};
            Vertex v2{{x1, planeY, z1}, col, norm};
            Vertex v3{{x0, planeY, z1}, col, norm};
            addQuad(v0, v1, v2, v3);
        }
    }

    // 2. Center Rotating Torus Mesh
    const int segments = 36;
    const int tubeSegments = 24;
    const float mainRadius = 1.0f;
    const float tubeRadius = 0.35f;

    uint32_t torusBaseIdx = static_cast<uint32_t>(vertices.size());

    for (int i = 0; i <= segments; ++i) {
        float u = static_cast<float>(i) / segments * 2.0f * vk_math::PI;
        float cosU = std::cos(u);
        float sinU = std::sin(u);

        for (int j = 0; j <= tubeSegments; ++j) {
            float v = static_cast<float>(j) / tubeSegments * 2.0f * vk_math::PI;
            float cosV = std::cos(v);
            float sinV = std::sin(v);

            float x = (mainRadius + tubeRadius * cosV) * cosU;
            float y = tubeRadius * sinV + 0.2f; // Slight elevation
            float z = (mainRadius + tubeRadius * cosV) * sinU;

            vk_math::Vec3 center(mainRadius * cosU, 0.2f, mainRadius * sinU);
            vk_math::Vec3 normal = (vk_math::Vec3(x, y, z) - center).normalize();

            // Vibrant gradient color along the torus circumference
            vk_math::Vec3 col(
                0.5f + 0.5f * std::sin(u),
                0.5f + 0.5f * std::sin(u + 2.094f),
                0.5f + 0.5f * std::sin(u + 4.188f)
            );

            vertices.push_back({{x, y, z}, col, normal});
        }
    }

    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < tubeSegments; ++j) {
            uint32_t first = torusBaseIdx + (i * (tubeSegments + 1)) + j;
            uint32_t second = first + tubeSegments + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    // 3. Surrounding Floating Geometric Cubes
    auto addCube = [&](const vk_math::Vec3& center, float size, const vk_math::Vec3& col) {
        float h = size * 0.5f;
        // Front
        addQuad(
            {{center.x - h, center.y - h, center.z + h}, col, {0.0f, 0.0f, 1.0f}},
            {{center.x + h, center.y - h, center.z + h}, col, {0.0f, 0.0f, 1.0f}},
            {{center.x + h, center.y + h, center.z + h}, col, {0.0f, 0.0f, 1.0f}},
            {{center.x - h, center.y + h, center.z + h}, col, {0.0f, 0.0f, 1.0f}}
        );
        // Back
        addQuad(
            {{center.x + h, center.y - h, center.z - h}, col, {0.0f, 0.0f, -1.0f}},
            {{center.x - h, center.y - h, center.z - h}, col, {0.0f, 0.0f, -1.0f}},
            {{center.x - h, center.y + h, center.z - h}, col, {0.0f, 0.0f, -1.0f}},
            {{center.x + h, center.y + h, center.z - h}, col, {0.0f, 0.0f, -1.0f}}
        );
        // Top
        addQuad(
            {{center.x - h, center.y + h, center.z + h}, col, {0.0f, 1.0f, 0.0f}},
            {{center.x + h, center.y + h, center.z + h}, col, {0.0f, 1.0f, 0.0f}},
            {{center.x + h, center.y + h, center.z - h}, col, {0.0f, 1.0f, 0.0f}},
            {{center.x - h, center.y + h, center.z - h}, col, {0.0f, 1.0f, 0.0f}}
        );
        // Bottom
        addQuad(
            {{center.x - h, center.y - h, center.z - h}, col, {0.0f, -1.0f, 0.0f}},
            {{center.x + h, center.y - h, center.z - h}, col, {0.0f, -1.0f, 0.0f}},
            {{center.x + h, center.y - h, center.z + h}, col, {0.0f, -1.0f, 0.0f}},
            {{center.x - h, center.y - h, center.z + h}, col, {0.0f, -1.0f, 0.0f}}
        );
        // Right
        addQuad(
            {{center.x + h, center.y - h, center.z + h}, col, {1.0f, 0.0f, 0.0f}},
            {{center.x + h, center.y - h, center.z - h}, col, {1.0f, 0.0f, 0.0f}},
            {{center.x + h, center.y + h, center.z - h}, col, {1.0f, 0.0f, 0.0f}},
            {{center.x + h, center.y + h, center.z + h}, col, {1.0f, 0.0f, 0.0f}}
        );
        // Left
        addQuad(
            {{center.x - h, center.y - h, center.z - h}, col, {-1.0f, 0.0f, 0.0f}},
            {{center.x - h, center.y - h, center.z + h}, col, {-1.0f, 0.0f, 0.0f}},
            {{center.x - h, center.y + h, center.z + h}, col, {-1.0f, 0.0f, 0.0f}},
            {{center.x - h, center.y + h, center.z - h}, col, {-1.0f, 0.0f, 0.0f}}
        );
    };

    addCube(vk_math::Vec3(-2.2f, -0.4f, -1.5f), 0.7f, vk_math::Vec3(0.95f, 0.25f, 0.25f));
    addCube(vk_math::Vec3( 2.2f, -0.4f,  1.5f), 0.7f, vk_math::Vec3(0.25f, 0.85f, 0.95f));
    addCube(vk_math::Vec3(-1.8f, -0.5f,  2.0f), 0.6f, vk_math::Vec3(0.95f, 0.85f, 0.15f));
    addCube(vk_math::Vec3( 1.8f, -0.5f, -2.0f), 0.6f, vk_math::Vec3(0.35f, 0.95f, 0.45f));
}

// ----------------------------------------------------------------------------
// Memory and Buffer Helpers
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
// Device Creation with Vulkan 1.4 Core Dynamic Rendering Local Reads
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

    // Vulkan 1.4 Dynamic Rendering Local Read Feature
    VkPhysicalDeviceDynamicRenderingLocalReadFeatures localReadFeatures{};
    localReadFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES;
    localReadFeatures.dynamicRenderingLocalRead = VK_TRUE;

    // Vulkan 1.3 Core Features
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
// G-Buffer Attachment Image Helper
// ----------------------------------------------------------------------------
struct AttachmentImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format;

    void destroy(VkDevice device) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
        if (image != VK_NULL_HANDLE) vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
    }
};

AttachmentImage createAttachmentImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect
) {
    AttachmentImage att{};
    att.format = format;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &att.image), "Failed to create attachment image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, att.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &att.memory), "Failed to allocate attachment memory");
    vk_common::check_vk_result(vkBindImageMemory(device, att.image, att.memory, 0), "Failed to bind attachment memory");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = att.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &att.view), "Failed to create attachment image view");

    return att;
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 8: Deferred Shading G-Buffer (Vulkan 1.4)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: Multiple Render Targets (MRT), Dynamic Rendering Local Reads, subpassLoad" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        // STEP 1: Window & Vulkan 1.4 Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 8: Deferred Shading MRT (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // STEP 2: Device with Dynamic Rendering Local Read enabled
        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = createDeviceWithLocalRead(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        PFN_vkCmdSetRenderingInputAttachmentIndices pfnSetRenderingInputAttachmentIndices = 
            (PFN_vkCmdSetRenderingInputAttachmentIndices)vkGetDeviceProcAddr(device, "vkCmdSetRenderingInputAttachmentIndices");
        if (!pfnSetRenderingInputAttachmentIndices) {
            pfnSetRenderingInputAttachmentIndices = (PFN_vkCmdSetRenderingInputAttachmentIndices)vkGetDeviceProcAddr(device, "vkCmdSetRenderingInputAttachmentIndicesKHR");
        }

        std::cout << "Vulkan 1.4 Logical Device initialized with Dynamic Rendering Local Read." << std::endl;

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

        // STEP 4: G-Buffer Attachments (Position, Normal, Albedo, Depth)
        // Position: RGBA16_SFLOAT (Color attachment 0 in Pass 1, Input attachment 0 in Pass 2)
        AttachmentImage gPosition = createAttachmentImage(
            device, physicalDevice, WIDTH, HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // Normal: RGBA16_SFLOAT (Color attachment 1 in Pass 1, Input attachment 1 in Pass 2)
        AttachmentImage gNormal = createAttachmentImage(
            device, physicalDevice, WIDTH, HEIGHT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // Albedo: RGBA8_UNORM (Color attachment 2 in Pass 1, Input attachment 2 in Pass 2)
        AttachmentImage gAlbedo = createAttachmentImage(
            device, physicalDevice, WIDTH, HEIGHT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // Depth: D32_SFLOAT (Depth buffer for Pass 1 geometry)
        AttachmentImage gDepth = createAttachmentImage(
            device, physicalDevice, WIDTH, HEIGHT,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );

        // STEP 5: Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 6: Scene Geometry Buffers
        std::vector<Vertex> sceneVertices;
        std::vector<uint32_t> sceneIndices;
        generateSceneGeometry(sceneVertices, sceneIndices);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * sceneVertices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexBufferMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, sceneVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);

        VkDeviceSize indexBufferSize = sizeof(uint32_t) * sceneIndices.size();
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexBufferMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, sceneIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 7: Uniform Buffers (Pass 1 Scene UBO + Pass 2 Light UBO)
        // Pass 1: Scene UBO
        VkDeviceSize sceneUBOSize = sizeof(SceneUBO);
        VkBuffer sceneUBOBuffer;
        VkDeviceMemory sceneUBOBufferMemory;
        void* sceneUBOMapped = nullptr;

        createBuffer(device, physicalDevice, sceneUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sceneUBOBuffer, sceneUBOBufferMemory);
        vkMapMemory(device, sceneUBOBufferMemory, 0, sceneUBOSize, 0, &sceneUBOMapped);

        // Pass 2: Light UBO
        VkDeviceSize lightUBOSize = sizeof(LightUBO);
        VkBuffer lightUBOBuffer;
        VkDeviceMemory lightUBOBufferMemory;
        void* lightUBOMapped = nullptr;

        createBuffer(device, physicalDevice, lightUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     lightUBOBuffer, lightUBOBufferMemory);
        vkMapMemory(device, lightUBOBufferMemory, 0, lightUBOSize, 0, &lightUBOMapped);

        // STEP 8: Descriptor Set Layouts & Pools
        // Pass 1 G-Buffer Descriptor Layout: Binding 0 = Scene UBO
        VkDescriptorSetLayoutBinding gbufferUBOBinding{};
        gbufferUBOBinding.binding = 0;
        gbufferUBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gbufferUBOBinding.descriptorCount = 1;
        gbufferUBOBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo gbufferLayoutInfo{};
        gbufferLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        gbufferLayoutInfo.bindingCount = 1;
        gbufferLayoutInfo.pBindings = &gbufferUBOBinding;

        VkDescriptorSetLayout gbufferDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &gbufferLayoutInfo, nullptr, &gbufferDescriptorSetLayout), "Failed to create G-Buffer descriptor set layout");

        // Pass 2 Lighting Descriptor Layout:
        // Binding 0 = inPosition (input attachment)
        // Binding 1 = inNormal   (input attachment)
        // Binding 2 = inAlbedo   (input attachment)
        // Binding 3 = LightUBO   (uniform buffer)
        std::array<VkDescriptorSetLayoutBinding, 4> lightingBindings{};
        for (uint32_t b = 0; b < 3; ++b) {
            lightingBindings[b].binding = b;
            lightingBindings[b].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            lightingBindings[b].descriptorCount = 1;
            lightingBindings[b].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        lightingBindings[3].binding = 3;
        lightingBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightingBindings[3].descriptorCount = 1;
        lightingBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo lightingLayoutInfo{};
        lightingLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lightingLayoutInfo.bindingCount = static_cast<uint32_t>(lightingBindings.size());
        lightingLayoutInfo.pBindings = lightingBindings.data();

        VkDescriptorSetLayout lightingDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &lightingLayoutInfo, nullptr, &lightingDescriptorSetLayout), "Failed to create Lighting descriptor set layout");

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        poolSizes[1].descriptorCount = 6;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();
        descPoolInfo.maxSets = 4;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        // Allocate Pass 1 Descriptor Set
        VkDescriptorSetAllocateInfo gbufferAllocInfo{};
        gbufferAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        gbufferAllocInfo.descriptorPool = descriptorPool;
        gbufferAllocInfo.descriptorSetCount = 1;
        gbufferAllocInfo.pSetLayouts = &gbufferDescriptorSetLayout;

        VkDescriptorSet gbufferDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &gbufferAllocInfo, &gbufferDescriptorSet), "Failed to allocate G-Buffer descriptor set");

        VkDescriptorBufferInfo sceneUBOBufferInfo{};
        sceneUBOBufferInfo.buffer = sceneUBOBuffer;
        sceneUBOBufferInfo.offset = 0;
        sceneUBOBufferInfo.range = sizeof(SceneUBO);

        VkWriteDescriptorSet gbufferWrite{};
        gbufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gbufferWrite.dstSet = gbufferDescriptorSet;
        gbufferWrite.dstBinding = 0;
        gbufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gbufferWrite.descriptorCount = 1;
        gbufferWrite.pBufferInfo = &sceneUBOBufferInfo;

        vkUpdateDescriptorSets(device, 1, &gbufferWrite, 0, nullptr);

        // Allocate Pass 2 Descriptor Set
        VkDescriptorSetAllocateInfo lightingAllocInfo{};
        lightingAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        lightingAllocInfo.descriptorPool = descriptorPool;
        lightingAllocInfo.descriptorSetCount = 1;
        lightingAllocInfo.pSetLayouts = &lightingDescriptorSetLayout;

        VkDescriptorSet lightingDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &lightingAllocInfo, &lightingDescriptorSet), "Failed to allocate lighting descriptor set");

        VkDescriptorImageInfo posImgInfo{VK_NULL_HANDLE, gPosition.view, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR};
        VkDescriptorImageInfo normImgInfo{VK_NULL_HANDLE, gNormal.view, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR};
        VkDescriptorImageInfo albImgInfo{VK_NULL_HANDLE, gAlbedo.view, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR};
        VkDescriptorBufferInfo lightUBOBufferInfo{lightUBOBuffer, 0, sizeof(LightUBO)};

        std::array<VkWriteDescriptorSet, 4> lightingWrites{};
        lightingWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[0].dstSet = lightingDescriptorSet;
        lightingWrites[0].dstBinding = 0;
        lightingWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingWrites[0].descriptorCount = 1;
        lightingWrites[0].pImageInfo = &posImgInfo;

        lightingWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[1].dstSet = lightingDescriptorSet;
        lightingWrites[1].dstBinding = 1;
        lightingWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingWrites[1].descriptorCount = 1;
        lightingWrites[1].pImageInfo = &normImgInfo;

        lightingWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[2].dstSet = lightingDescriptorSet;
        lightingWrites[2].dstBinding = 2;
        lightingWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        lightingWrites[2].descriptorCount = 1;
        lightingWrites[2].pImageInfo = &albImgInfo;

        lightingWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightingWrites[3].dstSet = lightingDescriptorSet;
        lightingWrites[3].dstBinding = 3;
        lightingWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightingWrites[3].descriptorCount = 1;
        lightingWrites[3].pBufferInfo = &lightUBOBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(lightingWrites.size()), lightingWrites.data(), 0, nullptr);

        // STEP 9: Pipeline Layouts
        // G-Buffer Pipeline Layout
        VkPipelineLayoutCreateInfo gbufferPipeLayoutInfo{};
        gbufferPipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        gbufferPipeLayoutInfo.setLayoutCount = 1;
        gbufferPipeLayoutInfo.pSetLayouts = &gbufferDescriptorSetLayout;

        VkPipelineLayout gbufferPipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &gbufferPipeLayoutInfo, nullptr, &gbufferPipelineLayout), "Failed to create G-Buffer pipeline layout");

        // Lighting Pipeline Layout
        VkPipelineLayoutCreateInfo lightingPipeLayoutInfo{};
        lightingPipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        lightingPipeLayoutInfo.setLayoutCount = 1;
        lightingPipeLayoutInfo.pSetLayouts = &lightingDescriptorSetLayout;

        VkPipelineLayout lightingPipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &lightingPipeLayoutInfo, nullptr, &lightingPipelineLayout), "Failed to create Lighting pipeline layout");

        // STEP 10: Shaders and Pipelines
        // Load G-Buffer shaders
        std::string gbVertPath = "shaders/gbuffer.vert.spv";
        std::string gbFragPath = "shaders/gbuffer.frag.spv";
        if (!std::filesystem::exists(gbVertPath)) gbVertPath = "assignment08_deferred_shading_g_buffer/shaders/gbuffer.vert.spv";
        if (!std::filesystem::exists(gbFragPath)) gbFragPath = "assignment08_deferred_shading_g_buffer/shaders/gbuffer.frag.spv";

        auto gbVertCode = vulkan_utils::readFile(gbVertPath);
        auto gbFragCode = vulkan_utils::readFile(gbFragPath);
        VkShaderModule gbVertModule = vulkan_utils::createShaderModule(device, gbVertCode);
        VkShaderModule gbFragModule = vulkan_utils::createShaderModule(device, gbFragCode);

        // Load Lighting shaders
        std::string lightVertPath = "shaders/deferred_lighting.vert.spv";
        std::string lightFragPath = "shaders/deferred_lighting.frag.spv";
        if (!std::filesystem::exists(lightVertPath)) lightVertPath = "assignment08_deferred_shading_g_buffer/shaders/deferred_lighting.vert.spv";
        if (!std::filesystem::exists(lightFragPath)) lightFragPath = "assignment08_deferred_shading_g_buffer/shaders/deferred_lighting.frag.spv";

        auto lightVertCode = vulkan_utils::readFile(lightVertPath);
        auto lightFragCode = vulkan_utils::readFile(lightFragPath);
        VkShaderModule lightVertModule = vulkan_utils::createShaderModule(device, lightVertCode);
        VkShaderModule lightFragModule = vulkan_utils::createShaderModule(device, lightFragCode);

        // ================================================================
        // Pipeline 1: G-Buffer Multiple Render Targets (MRT) Pipeline
        // Outputs to 3 Color Attachments (Position, Normal, Albedo) + Depth
        // ================================================================
        VkPipelineShaderStageCreateInfo gbShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, gbVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, gbFragModule, "main", nullptr}
        };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo gbVertexInput{};
        gbVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        gbVertexInput.vertexBindingDescriptionCount = 1;
        gbVertexInput.pVertexBindingDescriptions = &bindingDescription;
        gbVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        gbVertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

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

        // 3 Blend attachment states for MRT outputs
        std::array<VkPipelineColorBlendAttachmentState, 3> gbBlendAttachments{};
        for (auto& att : gbBlendAttachments) {
            att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            att.blendEnable = VK_FALSE;
        }

        VkPipelineColorBlendStateCreateInfo gbColorBlending{};
        gbColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        gbColorBlending.attachmentCount = static_cast<uint32_t>(gbBlendAttachments.size());
        gbColorBlending.pAttachments = gbBlendAttachments.data();

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 3 Color formats for G-Buffer MRT
        std::array<VkFormat, 3> gbColorFormats = {
            gPosition.format,
            gNormal.format,
            gAlbedo.format
        };

        VkPipelineRenderingCreateInfo gbRenderingInfo{};
        gbRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        gbRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(gbColorFormats.size());
        gbRenderingInfo.pColorAttachmentFormats = gbColorFormats.data();
        gbRenderingInfo.depthAttachmentFormat = gDepth.format;

        VkGraphicsPipelineCreateInfo gbPipelineInfo{};
        gbPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gbPipelineInfo.pNext = &gbRenderingInfo;
        gbPipelineInfo.stageCount = 2;
        gbPipelineInfo.pStages = gbShaderStages;
        gbPipelineInfo.pVertexInputState = &gbVertexInput;
        gbPipelineInfo.pInputAssemblyState = &inputAssembly;
        gbPipelineInfo.pViewportState = &viewportState;
        gbPipelineInfo.pRasterizationState = &rasterizer;
        gbPipelineInfo.pMultisampleState = &multisampling;
        gbPipelineInfo.pDepthStencilState = &depthStencil;
        gbPipelineInfo.pColorBlendState = &gbColorBlending;
        gbPipelineInfo.pDynamicState = &dynamicState;
        gbPipelineInfo.layout = gbufferPipelineLayout;

        VkPipeline gbufferPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gbPipelineInfo, nullptr, &gbufferPipeline), "Failed to create G-Buffer graphics pipeline");

        // ================================================================
        // Pipeline 2: Deferred Lighting with Dynamic Rendering Local Reads
        // Samples G-Buffer input attachments on-chip & renders to swapchain
        // ================================================================
        VkPipelineShaderStageCreateInfo lightShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, lightVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, lightFragModule, "main", nullptr}
        };

        VkPipelineVertexInputStateCreateInfo lightVertexInput{};
        lightVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineRasterizationStateCreateInfo lightRasterizer = rasterizer;
        lightRasterizer.cullMode = VK_CULL_MODE_NONE;

        VkPipelineDepthStencilStateCreateInfo lightDepthStencil{};
        lightDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        lightDepthStencil.depthTestEnable = VK_FALSE;
        lightDepthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState lightColorBlend{};
        lightColorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        lightColorBlend.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo lightColorBlending{};
        lightColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        lightColorBlending.attachmentCount = 1;
        lightColorBlending.pAttachments = &lightColorBlend;

        // Dynamic Rendering Local Read input attachment indices mapping
        std::array<uint32_t, 3> inputAttachmentIndices = {0, 1, 2};
        VkRenderingInputAttachmentIndexInfo inputAttachmentIndexInfo{};
        inputAttachmentIndexInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO;
        inputAttachmentIndexInfo.colorAttachmentCount = static_cast<uint32_t>(inputAttachmentIndices.size());
        inputAttachmentIndexInfo.pColorAttachmentInputIndices = inputAttachmentIndices.data();

        VkPipelineRenderingCreateInfo lightRenderingInfo{};
        lightRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        lightRenderingInfo.pNext = &inputAttachmentIndexInfo; // Chained dynamic rendering local read indices
        lightRenderingInfo.colorAttachmentCount = 1;
        lightRenderingInfo.pColorAttachmentFormats = &surfaceFormat.format;

        VkGraphicsPipelineCreateInfo lightPipelineInfo{};
        lightPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        lightPipelineInfo.pNext = &lightRenderingInfo;
        lightPipelineInfo.stageCount = 2;
        lightPipelineInfo.pStages = lightShaderStages;
        lightPipelineInfo.pVertexInputState = &lightVertexInput;
        lightPipelineInfo.pInputAssemblyState = &inputAssembly;
        lightPipelineInfo.pViewportState = &viewportState;
        lightPipelineInfo.pRasterizationState = &lightRasterizer;
        lightPipelineInfo.pMultisampleState = &multisampling;
        lightPipelineInfo.pDepthStencilState = &lightDepthStencil;
        lightPipelineInfo.pColorBlendState = &lightColorBlending;
        lightPipelineInfo.pDynamicState = &dynamicState;
        lightPipelineInfo.layout = lightingPipelineLayout;

        VkPipeline lightingPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &lightPipelineInfo, nullptr, &lightingPipeline), "Failed to create Lighting graphics pipeline");

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

        std::cout << "Deferred Shading G-Buffer Pipeline with Local Reads ready. Starting render loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        // STEP 12: Main Render Loop
        
        // Initialize Flame Graph Profiler for assignment08_deferred_shading_g_buffer
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment08_deferred_shading_g_buffer");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment08_deferred_shading_g_buffer");
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

            // Camera Orbit Calculation
            float camRadius = 4.2f;
            float camAngle = time * 0.35f;
            vk_math::Vec3 eyePos(std::cos(camAngle) * camRadius, 2.2f + std::sin(time * 0.2f) * 0.4f, std::sin(camAngle) * camRadius);

            // Update Scene UBO (Pass 1)
            SceneUBO sceneUbo{};
            sceneUbo.model = vk_math::Mat4::rotate(time * 0.6f, vk_math::Vec3(0.0f, 1.0f, 0.0f));
            sceneUbo.view = vk_math::Mat4::lookAt(eyePos, vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            sceneUbo.proj = vk_math::Mat4::perspective(
                vk_math::radians(50.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.1f,
                100.0f
            );
            sceneUbo.cameraPos[0] = eyePos.x;
            sceneUbo.cameraPos[1] = eyePos.y;
            sceneUbo.cameraPos[2] = eyePos.z;
            sceneUbo.cameraPos[3] = 1.0f;
            std::memcpy(sceneUBOMapped, &sceneUbo, sizeof(sceneUbo));

            // Update Light UBO (Pass 2): 6 Dynamic Orbiting Point Lights
            LightUBO lightUbo{};
            lightUbo.viewPos[0] = eyePos.x;
            lightUbo.viewPos[1] = eyePos.y;
            lightUbo.viewPos[2] = eyePos.z;
            lightUbo.viewPos[3] = 1.0f;
            lightUbo.lightCount = 6;
            lightUbo.displayMode = 0; // 0 = Full deferred lighting

            // Light 0: Orbiting Ruby Red
            float l0Angle = time * 1.2f;
            lightUbo.lights[0].position[0] = std::cos(l0Angle) * 2.0f;
            lightUbo.lights[0].position[1] = 0.5f + std::sin(time * 2.0f) * 0.5f;
            lightUbo.lights[0].position[2] = std::sin(l0Angle) * 2.0f;
            lightUbo.lights[0].position[3] = 4.0f; // Radius
            lightUbo.lights[0].color[0] = 1.0f; lightUbo.lights[0].color[1] = 0.15f; lightUbo.lights[0].color[2] = 0.15f; lightUbo.lights[0].color[3] = 2.5f;

            // Light 1: Orbiting Emerald Green
            float l1Angle = l0Angle + 2.094f;
            lightUbo.lights[1].position[0] = std::cos(l1Angle) * 2.0f;
            lightUbo.lights[1].position[1] = 0.5f + std::cos(time * 1.8f) * 0.5f;
            lightUbo.lights[1].position[2] = std::sin(l1Angle) * 2.0f;
            lightUbo.lights[1].position[3] = 4.0f;
            lightUbo.lights[1].color[0] = 0.15f; lightUbo.lights[1].color[1] = 1.0f; lightUbo.lights[1].color[2] = 0.35f; lightUbo.lights[1].color[3] = 2.5f;

            // Light 2: Orbiting Sapphire Blue
            float l2Angle = l0Angle + 4.188f;
            lightUbo.lights[2].position[0] = std::cos(l2Angle) * 2.0f;
            lightUbo.lights[2].position[1] = 0.5f + std::sin(time * 1.5f) * 0.5f;
            lightUbo.lights[2].position[2] = std::sin(l2Angle) * 2.0f;
            lightUbo.lights[2].position[3] = 4.0f;
            lightUbo.lights[2].color[0] = 0.2f; lightUbo.lights[2].color[1] = 0.45f; lightUbo.lights[2].color[2] = 1.0f; lightUbo.lights[2].color[3] = 2.5f;

            // Light 3: Inner Golden Core Light
            lightUbo.lights[3].position[0] = 0.0f;
            lightUbo.lights[3].position[1] = 0.2f + std::sin(time * 3.0f) * 0.3f;
            lightUbo.lights[3].position[2] = 0.0f;
            lightUbo.lights[3].position[3] = 3.0f;
            lightUbo.lights[3].color[0] = 1.0f; lightUbo.lights[3].color[1] = 0.85f; lightUbo.lights[3].color[2] = 0.2f; lightUbo.lights[3].color[3] = 3.0f;

            // Light 4 & 5: High Atmosphere Fill Lights
            lightUbo.lights[4].position[0] = -2.5f; lightUbo.lights[4].position[1] = 2.8f; lightUbo.lights[4].position[2] = -2.5f; lightUbo.lights[4].position[3] = 6.0f;
            lightUbo.lights[4].color[0] = 0.7f; lightUbo.lights[4].color[1] = 0.3f; lightUbo.lights[4].color[2] = 0.9f; lightUbo.lights[4].color[3] = 1.2f;

            lightUbo.lights[5].position[0] = 2.5f; lightUbo.lights[5].position[1] = 2.8f; lightUbo.lights[5].position[2] = 2.5f; lightUbo.lights[5].position[3] = 6.0f;
            lightUbo.lights[5].color[0] = 0.3f; lightUbo.lights[5].color[1] = 0.9f; lightUbo.lights[5].color[2] = 0.9f; lightUbo.lights[5].color[3] = 1.2f;

            std::memcpy(lightUBOMapped, &lightUbo, sizeof(lightUbo));

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // ================================================================
            // PASS 1: G-Buffer Generation (MRT Color Attachments + Depth)
            // ================================================================
            // Transition G-Buffer Attachments to COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gPosition.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gNormal.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gAlbedo.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Transition Depth Buffer
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gDepth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT
            );

            // Setup 3 MRT Color Attachments
            std::array<VkRenderingAttachmentInfo, 3> pass1ColorAttachments{};
            pass1ColorAttachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass1ColorAttachments[0].imageView = gPosition.view;
            pass1ColorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass1ColorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass1ColorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass1ColorAttachments[0].clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

            pass1ColorAttachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass1ColorAttachments[1].imageView = gNormal.view;
            pass1ColorAttachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass1ColorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass1ColorAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass1ColorAttachments[1].clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

            pass1ColorAttachments[2].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass1ColorAttachments[2].imageView = gAlbedo.view;
            pass1ColorAttachments[2].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass1ColorAttachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass1ColorAttachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass1ColorAttachments[2].clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = gDepth.view;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            VkRenderingInfo pass1Rendering{};
            pass1Rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            pass1Rendering.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            pass1Rendering.layerCount = 1;
            pass1Rendering.colorAttachmentCount = static_cast<uint32_t>(pass1ColorAttachments.size());
            pass1Rendering.pColorAttachments = pass1ColorAttachments.data();
            pass1Rendering.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &pass1Rendering);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipelineLayout, 0, 1, &gbufferDescriptorSet, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(sceneIndices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // ================================================================
            // INTER-PASS SYNCHRONIZATION: PipelineBarrier2 for Dynamic Local Reads
            // Transition G-Buffer Attachments to RENDERING_LOCAL_READ layout
            // ================================================================
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gPosition.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gNormal.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                gAlbedo.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Transition Swapchain presentation image to COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // ================================================================
            // PASS 2: Deferred Lighting Pass (Dynamic Rendering Local Read)
            // ================================================================
            VkRenderingAttachmentInfo pass2ColorAttachment{};
            pass2ColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            pass2ColorAttachment.imageView = swapchainImageViews[imageIndex];
            pass2ColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            pass2ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            pass2ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            pass2ColorAttachment.clearValue.color = {{0.02f, 0.03f, 0.06f, 1.0f}};

            VkRenderingInfo pass2Rendering{};
            pass2Rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            pass2Rendering.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            pass2Rendering.layerCount = 1;
            pass2Rendering.colorAttachmentCount = 1;
            pass2Rendering.pColorAttachments = &pass2ColorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &pass2Rendering);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline);

            if (pfnSetRenderingInputAttachmentIndices) {
                std::array<uint32_t, 3> dynamicInputIndices = {0, 1, 2};
                VkRenderingInputAttachmentIndexInfo dynInputIdxInfo{};
                dynInputIdxInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO;
                dynInputIdxInfo.colorAttachmentCount = static_cast<uint32_t>(dynamicInputIndices.size());
                dynInputIdxInfo.pColorAttachmentInputIndices = dynamicInputIndices.data();
                pfnSetRenderingInputAttachmentIndices(commandBuffer, &dynInputIdxInfo);
            }

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Bind Pass 2 Descriptor Set containing 3 G-Buffer Local Read attachments + Light UBO
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout, 0, 1, &lightingDescriptorSet, 0, nullptr);

            // Draw fullscreen triangle (3 vertices generated via gl_VertexIndex)
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // Transition Swapchain Image for Presentation
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer, vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE,
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
        profiler.exportFoldedFile("flamegraph_assignment08_deferred_shading_g_buffer.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment08_deferred_shading_g_buffer.html");
        profiler.exportChromeTraceFile("flamegraph_assignment08_deferred_shading_g_buffer.json");
        profiler.cleanupGpu();


        // STEP 13: Cleanup Resources
        vkUnmapMemory(device, lightUBOBufferMemory);
        vkDestroyBuffer(device, lightUBOBuffer, nullptr);
        vkFreeMemory(device, lightUBOBufferMemory, nullptr);

        vkUnmapMemory(device, sceneUBOBufferMemory);
        vkDestroyBuffer(device, sceneUBOBuffer, nullptr);
        vkFreeMemory(device, sceneUBOBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, gbufferDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, lightingDescriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        gPosition.destroy(device);
        gNormal.destroy(device);
        gAlbedo.destroy(device);
        gDepth.destroy(device);

        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyPipeline(device, gbufferPipeline, nullptr);
        vkDestroyPipelineLayout(device, gbufferPipelineLayout, nullptr);
        vkDestroyPipeline(device, lightingPipeline, nullptr);
        vkDestroyPipelineLayout(device, lightingPipelineLayout, nullptr);

        vkDestroyShaderModule(device, gbVertModule, nullptr);
        vkDestroyShaderModule(device, gbFragModule, nullptr);
        vkDestroyShaderModule(device, lightVertModule, nullptr);
        vkDestroyShaderModule(device, lightFragModule, nullptr);

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

    std::cout << "Assignment 8 executed cleanly." << std::endl;
    return EXIT_SUCCESS;
}
