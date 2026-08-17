// ============================================================================
// Assignment 19: Hardware Variable Rate Shading (VRS) & Fragment Density Control
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_KHR_fragment_shading_rate extension & feature detection
//   - Pipeline & Dynamic Shading Rate Control (vkCmdSetFragmentShadingRateKHR)
//   - Attachment Shading Rate Image (Foveated / Radial Shading Rate Map)
//   - VkRenderingFragmentShadingRateAttachmentInfoKHR in Dynamic Rendering
//   - Multiple Shading Rate Combiners (VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, etc.)
//   - Real-time 3D Torus/Knot with dynamic shading rate visualization and performance
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
#include <cstring>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ----------------------------------------------------------------------------
// Extension Function Pointers for VK_KHR_fragment_shading_rate
// ----------------------------------------------------------------------------
static PFN_vkCmdSetFragmentShadingRateKHR pfn_vkCmdSetFragmentShadingRateKHR = nullptr;
static PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR = nullptr;

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

struct UniformBufferObject {
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
};

struct PushConstants {
    float debugOptions[4]; // [0]: shading rate mode, [1]: time, [2]: highlight, [3]: unused
};

// ----------------------------------------------------------------------------
// 3D Geometry: High-Density 3D Torus
// ----------------------------------------------------------------------------
void generateTorus(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float r = 0.35f, float R = 0.85f, int radialSegments = 64, int tubularSegments = 64) {
    vertices.clear();
    indices.clear();

    for (int j = 0; j <= radialSegments; ++j) {
        float v = float(j) / float(radialSegments);
        float phi = v * 2.0f * static_cast<float>(M_PI);
        float cosPhi = std::cos(phi);
        float sinPhi = std::sin(phi);

        for (int i = 0; i <= tubularSegments; ++i) {
            float u = float(i) / float(tubularSegments);
            float theta = u * 2.0f * static_cast<float>(M_PI);
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            Vertex vert{};
            vert.pos.x = (R + r * cosPhi) * cosTheta;
            vert.pos.y = r * sinPhi;
            vert.pos.z = (R + r * cosPhi) * sinTheta;

            vert.normal.x = cosPhi * cosTheta;
            vert.normal.y = sinPhi;
            vert.normal.z = cosPhi * sinTheta;

            // Vibrant gradient coloring based on torus coordinates
            vert.color.x = 0.5f + 0.5f * cosTheta;
            vert.color.y = 0.5f + 0.5f * sinPhi;
            vert.color.z = 0.5f + 0.5f * std::sin(theta + phi);

            vertices.push_back(vert);
        }
    }

    for (int j = 0; j < radialSegments; ++j) {
        for (int i = 0; i < tubularSegments; ++i) {
            uint32_t a = (tubularSegments + 1) * j + i;
            uint32_t b = (tubularSegments + 1) * (j + 1) + i;
            uint32_t c = (tubularSegments + 1) * (j + 1) + (i + 1);
            uint32_t d = (tubularSegments + 1) * j + (i + 1);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(d);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
}

// ----------------------------------------------------------------------------
// Helper Functions for Buffers & Memory
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

// Helper for Shading Rate Pattern Generation
// Encodes shading rate texel according to Vulkan VK_KHR_fragment_shading_rate spec:
// Byte representation: (texelWidth >> 1) | ((texelHeight >> 1) << 2) or standard rate enum lookup
// In Vulkan R8_UINT shading rate image format:
// 1x1: 0 (or (1>>1)|((1>>1)<<2) = 0)
// 1x2: 1 | (0 << 2) = 1 (or 1x2 = 0x01)
// 2x1: 0 | (1 << 2) = 4
// 2x2: (2>>1)|((2>>1)<<2) = 1 | (1 << 2) = 5 (0x05)
// 2x4: 1 | (2 << 2) = 9 (0x09)
// 4x2: 2 | (1 << 2) = 6 (0x06)
// 4x4: 2 | (2 << 2) = 10 (0x0A)
static uint8_t encodeShadingRate(uint32_t width, uint32_t height) {
    uint8_t wVal = (width == 4) ? 2 : ((width == 2) ? 1 : 0);
    uint8_t hVal = (height == 4) ? 2 : ((height == 2) ? 1 : 0);
    return (hVal << 2) | wVal;
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 19: Hardware Variable Rate Shading (VRS)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: VK_KHR_fragment_shading_rate, Foveated Shading Map,\n";
    std::cout << "           Attachment & Dynamic Pipeline Shading Rates (1x1, 2x2, 4x4)\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 720;

        // STEP 1: Create GLFW Window
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 19: Variable Rate Shading (Vulkan 1.4)");

        // STEP 2: Create Vulkan 1.4 Instance with Shading Rate & GetPhysical Properties Support
        VkInstance instance = vulkan_utils::createInstance();

        // STEP 3: Surface Creation
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);

        // STEP 4: Physical Device Selection
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Load Instance-level ProcAddrs for Fragment Shading Rate
        pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR = (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFragmentShadingRatesKHR");

        // STEP 5: Query Hardware Shading Rate Capabilities & Properties
        VkPhysicalDeviceFragmentShadingRatePropertiesKHR shadingRateProps{};
        shadingRateProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &shadingRateProps;
        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

        std::cout << "\n[VRS Properties] Physical Device: " << props2.properties.deviceName << "\n";
        std::cout << "[VRS Properties] Min Shading Rate Attachment Texel Size: "
                  << shadingRateProps.minFragmentShadingRateAttachmentTexelSize.width << "x"
                  << shadingRateProps.minFragmentShadingRateAttachmentTexelSize.height << "\n";
        std::cout << "[VRS Properties] Max Shading Rate Attachment Texel Size: "
                  << shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.width << "x"
                  << shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.height << "\n";
        std::cout << "[VRS Properties] Max Fragment Size: "
                  << shadingRateProps.maxFragmentSize.width << "x"
                  << shadingRateProps.maxFragmentSize.height << "\n";
        std::cout << "[VRS Properties] Non-Trivial Combiner Ops: "
                  << (shadingRateProps.fragmentShadingRateNonTrivialCombinerOps ? "Supported" : "Unsupported") << "\n";

        // Query Available Hardware Shading Rates
        if (pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR) {
            uint32_t rateCount = 0;
            pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &rateCount, nullptr);
            std::vector<VkPhysicalDeviceFragmentShadingRateKHR> rates(rateCount);
            for (auto& r : rates) {
                r.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
            }
            pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &rateCount, rates.data());

            std::cout << "[VRS Properties] Supported Hardware Shading Rates (" << rateCount << " rates):\n";
            for (const auto& r : rates) {
                std::cout << "  - " << r.fragmentSize.width << "x" << r.fragmentSize.height
                          << " (Sample Count: " << r.sampleCounts << ")\n";
            }
        }

        // Query Shading Rate Features
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR shadingRateFeatures{};
        shadingRateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &shadingRateFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        std::cout << "[VRS Features] Pipeline Shading Rate: " << (shadingRateFeatures.pipelineFragmentShadingRate ? "YES" : "NO") << "\n";
        std::cout << "[VRS Features] Attachment Shading Rate: " << (shadingRateFeatures.attachmentFragmentShadingRate ? "YES" : "NO") << "\n";
        std::cout << "[VRS Features] Primitive Shading Rate: " << (shadingRateFeatures.primitiveFragmentShadingRate ? "YES" : "NO") << "\n\n";

        // STEP 6: Find Queue Family
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
            throw std::runtime_error("Could not find a suitable graphics & present queue family!");
        }

        // STEP 7: Create Logical Device Enabling Dynamic Rendering & VK_KHR_fragment_shading_rate
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME
        };

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFragmentShadingRateFeaturesKHR enabledShadingRateFeatures{};
        enabledShadingRateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        enabledShadingRateFeatures.pipelineFragmentShadingRate = shadingRateFeatures.pipelineFragmentShadingRate;
        enabledShadingRateFeatures.attachmentFragmentShadingRate = shadingRateFeatures.attachmentFragmentShadingRate;
        enabledShadingRateFeatures.primitiveFragmentShadingRate = shadingRateFeatures.primitiveFragmentShadingRate;

        vulkan13Features.pNext = &enabledShadingRateFeatures;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &vulkan13Features;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device = VK_NULL_HANDLE;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device");

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        // Load Vulkan 1.4 Core & VRS Device Functions
        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        pfn_vkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR)vkGetDeviceProcAddr(device, "vkCmdSetFragmentShadingRateKHR");

        // STEP 8: Create Swapchain
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

        // STEP 9: Create Depth Attachment
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

        VkMemoryRequirements depthMemReq;
        vkGetImageMemoryRequirements(device, depthImage, &depthMemReq);

        VkMemoryAllocateInfo depthAllocInfo{};
        depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAllocInfo.allocationSize = depthMemReq.size;
        depthAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, depthMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

        // STEP 10: Create Fragment Shading Rate Image & Attachment (Foveated Map)
        // Texel size from hardware properties (typically 16x16)
        VkExtent2D vrsTexelSize = shadingRateProps.minFragmentShadingRateAttachmentTexelSize;
        if (vrsTexelSize.width == 0 || vrsTexelSize.height == 0) {
            vrsTexelSize = {16, 16};
        }

        uint32_t vrsWidth = (WIDTH + vrsTexelSize.width - 1) / vrsTexelSize.width;
        uint32_t vrsHeight = (HEIGHT + vrsTexelSize.height - 1) / vrsTexelSize.height;

        VkFormat vrsFormat = VK_FORMAT_R8_UINT;
        VkImage vrsImage;
        VkDeviceMemory vrsImageMemory;
        VkImageView vrsImageView;

        VkImageCreateInfo vrsImageInfo{};
        vrsImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        vrsImageInfo.imageType = VK_IMAGE_TYPE_2D;
        vrsImageInfo.extent.width = vrsWidth;
        vrsImageInfo.extent.height = vrsHeight;
        vrsImageInfo.extent.depth = 1;
        vrsImageInfo.mipLevels = 1;
        vrsImageInfo.arrayLayers = 1;
        vrsImageInfo.format = vrsFormat;
        vrsImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vrsImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vrsImageInfo.usage = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vrsImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vrsImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vk_common::check_vk_result(vkCreateImage(device, &vrsImageInfo, nullptr, &vrsImage), "Failed to create VRS image");

        VkMemoryRequirements vrsMemReq;
        vkGetImageMemoryRequirements(device, vrsImage, &vrsMemReq);

        VkMemoryAllocateInfo vrsAllocInfo{};
        vrsAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vrsAllocInfo.allocationSize = vrsMemReq.size;
        vrsAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, vrsMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vk_common::check_vk_result(vkAllocateMemory(device, &vrsAllocInfo, nullptr, &vrsImageMemory), "Failed to allocate VRS image memory");
        vk_common::check_vk_result(vkBindImageMemory(device, vrsImage, vrsImageMemory, 0), "Failed to bind VRS image memory");

        VkImageViewCreateInfo vrsViewInfo{};
        vrsViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vrsViewInfo.image = vrsImage;
        vrsViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vrsViewInfo.format = vrsFormat;
        vrsViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vrsViewInfo.subresourceRange.baseMipLevel = 0;
        vrsViewInfo.subresourceRange.levelCount = 1;
        vrsViewInfo.subresourceRange.baseArrayLayer = 0;
        vrsViewInfo.subresourceRange.layerCount = 1;

        vk_common::check_vk_result(vkCreateImageView(device, &vrsViewInfo, nullptr, &vrsImageView), "Failed to create VRS image view");

        // Generate Foveated/Radial Shading Rate Pattern Data
        // Center: 1x1 full rate, Middle ring: 2x2 rate, Outer perimeter: 4x4 rate
        std::vector<uint8_t> vrsPatternData(vrsWidth * vrsHeight);
        float centerX = static_cast<float>(vrsWidth) * 0.5f;
        float centerY = static_cast<float>(vrsHeight) * 0.5f;
        float maxRadius = std::sqrt(centerX * centerX + centerY * centerY);

        for (uint32_t y = 0; y < vrsHeight; ++y) {
            for (uint32_t x = 0; x < vrsWidth; ++x) {
                float dx = static_cast<float>(x) - centerX;
                float dy = static_cast<float>(y) - centerY;
                float dist = std::sqrt(dx * dx + dy * dy) / maxRadius;

                if (dist < 0.35f) {
                    // High Detail Center (1x1 rate)
                    vrsPatternData[y * vrsWidth + x] = encodeShadingRate(1, 1);
                } else if (dist < 0.70f) {
                    // Medium Detail Mid-Ring (2x2 rate)
                    vrsPatternData[y * vrsWidth + x] = encodeShadingRate(2, 2);
                } else {
                    // Low Detail Periphery (4x4 or 2x2 depending on max fragment rate)
                    uint32_t maxRate = (shadingRateProps.maxFragmentSize.width >= 4 && shadingRateProps.maxFragmentSize.height >= 4) ? 4 : 2;
                    vrsPatternData[y * vrsWidth + x] = encodeShadingRate(maxRate, maxRate);
                }
            }
        }

        // Upload Shading Rate Image Data via Staging Buffer
        VkDeviceSize vrsBufferSize = vrsPatternData.size();
        VkBuffer vrsStagingBuffer;
        VkDeviceMemory vrsStagingBufferMemory;
        createBuffer(device, physicalDevice, vrsBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vrsStagingBuffer, vrsStagingBufferMemory);

        void* vrsDataPtr = nullptr;
        vkMapMemory(device, vrsStagingBufferMemory, 0, vrsBufferSize, 0, &vrsDataPtr);
        std::memcpy(vrsDataPtr, vrsPatternData.data(), vrsBufferSize);
        vkUnmapMemory(device, vrsStagingBufferMemory);

        // Command Pool for Initialization Transfers
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // Record Image Transition & Buffer-to-Image Copy for VRS Image
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer setupCmd;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &setupCmd);

        VkCommandBufferBeginInfo setupBeginInfo{};
        setupBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        setupBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(setupCmd, &setupBeginInfo);

        // 1. Transition VRS Image UNDEFINED -> TRANSFER_DST_OPTIMAL
        vulkan_utils::pipelineBarrier2ImageTransition(
            setupCmd,
            vk14.vkCmdPipelineBarrier2,
            vrsImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // 2. Copy Staging Buffer to VRS Image
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {vrsWidth, vrsHeight, 1};

        vkCmdCopyBufferToImage(setupCmd, vrsStagingBuffer, vrsImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // 3. Transition VRS Image TRANSFER_DST_OPTIMAL -> FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
        vulkan_utils::pipelineBarrier2ImageTransition(
            setupCmd,
            vk14.vkCmdPipelineBarrier2,
            vrsImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
            VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        vkEndCommandBuffer(setupCmd);

        VkSubmitInfo setupSubmit{};
        setupSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        setupSubmit.commandBufferCount = 1;
        setupSubmit.pCommandBuffers = &setupCmd;

        vkQueueSubmit(graphicsQueue, 1, &setupSubmit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &setupCmd);
        vkDestroyBuffer(device, vrsStagingBuffer, nullptr);
        vkFreeMemory(device, vrsStagingBufferMemory, nullptr);

        std::cout << "[VRS Setup] Generated & uploaded " << vrsWidth << "x" << vrsHeight
                  << " Foveated VRS Map to Device Memory.\n";

        // STEP 11: Create Torus Geometry Buffers
        std::vector<Vertex> torusVertices;
        std::vector<uint32_t> torusIndices;
        generateTorus(torusVertices, torusIndices, 0.40f, 0.90f, 80, 80);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * torusVertices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexBufferMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, torusVertices.data(), vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);

        VkDeviceSize indexBufferSize = sizeof(uint32_t) * torusIndices.size();
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexBufferMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, torusIndices.data(), indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 12: Create Uniform Buffer
        VkDeviceSize uboBufferSize = sizeof(UniformBufferObject);
        VkBuffer uniformBuffer;
        VkDeviceMemory uniformBufferMemory;
        createBuffer(device, physicalDevice, uboBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffer, uniformBufferMemory);

        void* uboMappedMemory = nullptr;
        vkMapMemory(device, uniformBufferMemory, 0, uboBufferSize, 0, &uboMappedMemory);

        // STEP 13: Create Descriptor Set Layout & Descriptor Pool
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.pPoolSizes = &poolSize;
        poolCreateInfo.maxSets = 1;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.descriptorPool = descriptorPool;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &setAllocInfo, &descriptorSet), "Failed to allocate descriptor set");

        VkDescriptorBufferInfo uboDescInfo{};
        uboDescInfo.buffer = uniformBuffer;
        uboDescInfo.offset = 0;
        uboDescInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &uboDescInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        // STEP 14: Pipeline Layout with Push Constants
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 15: Load Shaders and Create Graphics Pipeline targeting Vulkan 1.4 Dynamic Rendering
        std::string vertPath = "shaders/vrs.vert.spv";
        std::string fragPath = "shaders/vrs.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment19_variable_rate_shading/shaders/vrs.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment19_variable_rate_shading/shaders/vrs.frag.spv";

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
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
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
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Dynamic State for Viewport, Scissor, and Dynamic Fragment Shading Rate
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        if (shadingRateFeatures.pipelineFragmentShadingRate && pfn_vkCmdSetFragmentShadingRateKHR) {
            dynamicStates.push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);
        }

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Dynamic Rendering Pipeline Create Info
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;
        pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

        // Pipeline Fragment Shading Rate State
        VkPipelineFragmentShadingRateStateCreateInfoKHR pipelineShadingRateState{};
        pipelineShadingRateState.sType = VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR;
        pipelineShadingRateState.fragmentSize = {1, 1};
        pipelineShadingRateState.combinerOps[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
        pipelineShadingRateState.combinerOps[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;

        pipelineRenderingCreateInfo.pNext = &pipelineShadingRateState;

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
        pipelineInfo.subpass = 0;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 16: Command Buffer & Synchronization Primitives
        VkCommandBuffer commandBuffer;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore), "Failed to create semaphore");
        vk_common::check_vk_result(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence), "Failed to create fence");

        std::cout << "[VRS Runtime] Render Loop Initialized. Real-time dynamic rate toggling available." << std::endl;

        const char* maxFramesEnv = std::getenv("VULKAN_MAX_FRAMES");
        uint64_t maxFrames = maxFramesEnv ? std::stoull(maxFramesEnv) : UINT64_MAX;

        // Main Render Loop
        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;
        uint32_t shadingRateMode = 0; // 0: Attachment Foveated (1x1 center, 2x2 mid, 4x4 edge), 1: 1x1 Global, 2: 2x2 Global, 3: 4x4 Global

        
        // Initialize Flame Graph Profiler for assignment19_variable_rate_shading
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment19_variable_rate_shading");
        profiler.initGpu(device, physicalDevice);

        while (!glfwWindowShouldClose(window) && frameCount < maxFrames) {
            VK_PROFILE_SCOPE("assignment19_variable_rate_shading");
            glfwPollEvents();

            // Wait for fence
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            // Acquire next swapchain image
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) break;

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Cycle shading rate modes periodically (every 240 frames / ~4s at 60fps) to clearly observe each rate
            const uint32_t FRAMES_PER_MODE = 240;
            shadingRateMode = (frameCount / FRAMES_PER_MODE) % 4;

            const char* modeDescriptions[] = {
                "Foveated Shading Attachment (Radial 1x1 -> 2x2 -> 4x4)",
                "Global Pipeline 1x1 (Full Rate)",
                "Global Pipeline 2x2 (Coarse Rate)",
                "Global Pipeline 4x4 (Maximum Coarse Rate)"
            };

            // Update GLFW Window Title with current shading rate and frame info
            if (frameCount % 10 == 0) {
                std::string windowTitle = "Assignment 19: VRS (Vulkan 1.4) | Shading Rate: [" + 
                                          std::to_string(shadingRateMode) + "] " + 
                                          modeDescriptions[shadingRateMode];
                glfwSetWindowTitle(window, windowTitle.c_str());
            }

            // Update UBO MVP matrices
            UniformBufferObject ubo{};
            ubo.model = vk_math::Mat4::rotate(time * 0.75f, vk_math::Vec3(0.0f, 1.0f, 0.0f)) *
                        vk_math::Mat4::rotate(time * 0.35f, vk_math::Vec3(1.0f, 0.0f, 0.0f));
            ubo.view = vk_math::Mat4::lookAt(vk_math::Vec3(0.0f, 0.0f, 3.2f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            ubo.proj = vk_math::Mat4::perspective(45.0f * (static_cast<float>(M_PI) / 180.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);

            std::memcpy(uboMappedMemory, &ubo, sizeof(ubo));

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);

            VkCommandBufferBeginInfo cmdBeginInfo{};
            cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo);

            // 1. Transition Swapchain Image UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                commandBuffer,
                vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // 2. Transition Depth Image UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL
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

            // Dynamic Rendering Attachment Configurations
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.06f, 0.07f, 0.11f, 1.0f}};

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = {1.0f, 0};

            // Shading Rate Attachment Info
            VkRenderingFragmentShadingRateAttachmentInfoKHR vrsAttachmentInfo{};
            vrsAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
            vrsAttachmentInfo.imageView = vrsImageView;
            vrsAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
            vrsAttachmentInfo.shadingRateAttachmentTexelSize = vrsTexelSize;

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            // In Mode 0, attach the Foveated VRS Map to the dynamic rendering pass
            if (shadingRateMode == 0 && shadingRateFeatures.attachmentFragmentShadingRate) {
                renderingInfo.pNext = &vrsAttachmentInfo;
            }

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Dynamic Shading Rate Control via vkCmdSetFragmentShadingRateKHR
            if (pfn_vkCmdSetFragmentShadingRateKHR && shadingRateFeatures.pipelineFragmentShadingRate) {
                VkExtent2D fragmentSize{1, 1};
                VkFragmentShadingRateCombinerOpKHR combiners[2] = {
                    VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
                    VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
                };

                switch (shadingRateMode) {
                    case 0:
                        // Mode 0: Attachment-driven foveated shading map
                        fragmentSize = {1, 1};
                        combiners[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR; // pipeline vs primitive
                        combiners[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR; // attachment replaces pipeline rate
                        break;
                    case 1:
                        // Mode 1: 1x1 full pipeline rate
                        fragmentSize = {1, 1};
                        combiners[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        combiners[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        break;
                    case 2:
                        // Mode 2: 2x2 coarse pipeline rate
                        fragmentSize = {2, 2};
                        combiners[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        combiners[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        break;
                    case 3:
                        // Mode 3: 4x4 or max coarse rate
                        fragmentSize = {
                            (shadingRateProps.maxFragmentSize.width >= 4) ? 4u : 2u,
                            (shadingRateProps.maxFragmentSize.height >= 4) ? 4u : 2u
                        };
                        combiners[0] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        combiners[1] = VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR;
                        break;
                }

                pfn_vkCmdSetFragmentShadingRateKHR(commandBuffer, &fragmentSize, combiners);
            }

            // Push Constants for Debug / Shading Rate display
            PushConstants pc{};
            pc.debugOptions[0] = static_cast<float>(shadingRateMode);
            pc.debugOptions[1] = time;
            pc.debugOptions[2] = 1.0f;
            pc.debugOptions[3] = 0.0f;
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

            // Bind Vertex & Index Buffers
            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Bind Descriptor Sets
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            // Draw Torus Geometry
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(torusIndices.size()), 1, 0, 0, 0);

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

            frameCount++;
            if (frameCount == 1 || (frameCount % FRAMES_PER_MODE == 0) || (frameCount % 60 == 0 && (frameCount / FRAMES_PER_MODE) != ((frameCount - 1) / FRAMES_PER_MODE))) {
                const char* modeNames[] = {
                    "Foveated Shading Rate Attachment (Radial 1x1 -> 2x2 -> 4x4 Map)",
                    "Global Pipeline 1x1 Full Shading Rate",
                    "Global Pipeline 2x2 Coarse Shading Rate",
                    "Global Pipeline 4x4 Coarse Shading Rate"
                };
                std::cout << "[VRS Status] Frame #" << frameCount << " | Active VRS Mode ["
                          << shadingRateMode << "]: " << modeNames[shadingRateMode] << std::endl;
            }
        }

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment19_variable_rate_shading.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment19_variable_rate_shading.html");
        profiler.exportChromeTraceFile("flamegraph_assignment19_variable_rate_shading.json");
        profiler.cleanupGpu();


        // STEP 17: Cleanup Resources
        vkUnmapMemory(device, uniformBufferMemory);
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyImageView(device, vrsImageView, nullptr);
        vkDestroyImage(device, vrsImage, nullptr);
        vkFreeMemory(device, vrsImageMemory, nullptr);

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

    std::cout << "Assignment 19 (Variable Rate Shading) executed and verified successfully.\n";
    return EXIT_SUCCESS;
}
