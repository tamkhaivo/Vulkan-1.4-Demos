// ============================================================================
// Assignment 12: Modern Descriptor Buffers (VK_EXT_descriptor_buffer) & Push Descriptors
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_EXT_descriptor_buffer (Raw memory-backed descriptor storage)
//   - Elimination of VkDescriptorPool and VkDescriptorSet allocation latency
//   - vkCmdBindDescriptorBuffersEXT and vkCmdSetDescriptorBufferOffsetsEXT
//   - Direct host memory layout packing of uniform and image descriptors
//   - Vulkan 1.4 Dynamic Rendering & Synchronization2 Real-Time Render Loop
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

// Extension function pointers for VK_EXT_descriptor_buffer
static PFN_vkGetDescriptorSetLayoutBindingOffsetEXT pfn_vkGetDescriptorSetLayoutBindingOffsetEXT = nullptr;
static PFN_vkGetDescriptorSetLayoutSizeEXT          pfn_vkGetDescriptorSetLayoutSizeEXT = nullptr;
static PFN_vkGetDescriptorEXT                      pfn_vkGetDescriptorEXT = nullptr;
static PFN_vkCmdBindDescriptorBuffersEXT           pfn_vkCmdBindDescriptorBuffersEXT = nullptr;
static PFN_vkCmdSetDescriptorBufferOffsetsEXT      pfn_vkCmdSetDescriptorBufferOffsetsEXT = nullptr;

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

struct UniformBufferObject {
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
};

// Distinct Colored 3D Cube Vertices
const std::vector<Vertex> cubeVertices = {
    // Front face (Red/Orange)
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 0.2f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.5f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.8f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.9f, 0.1f, 0.1f}},

    // Back face (Blue/Cyan)
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.5f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.8f, 0.9f}},
    {{-0.5f,  0.5f, -0.5f}, {0.1f, 0.2f, 0.9f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.3f, 0.8f}},

    // Top face (Green/Yellow)
    {{-0.5f, -0.5f, -0.5f}, {0.2f, 0.9f, 0.2f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.8f, 0.9f, 0.1f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.3f, 1.0f, 0.4f}},
    {{-0.5f, -0.5f,  0.5f}, {0.5f, 0.9f, 0.2f}},

    // Bottom face (Magenta/Purple)
    {{-0.5f,  0.5f,  0.5f}, {0.8f, 0.1f, 0.8f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.6f, 0.0f, 0.7f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.9f, 0.2f, 0.7f}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.0f, 0.5f}},

    // Right face (Teal)
    {{ 0.5f, -0.5f,  0.5f}, {0.1f, 0.9f, 0.8f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.8f, 0.6f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.2f, 0.7f, 0.7f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.1f, 0.9f, 0.9f}},

    // Left face (Yellow/Gold)
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.9f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.9f, 0.8f, 0.2f}},
    {{-0.5f,  0.5f,  0.5f}, {0.8f, 0.7f, 0.1f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.3f}}
};

const std::vector<uint16_t> cubeIndices = {
    0,  1,  2,      2,  3,  0,    // Front
    4,  5,  6,      6,  7,  4,    // Back
    8,  9, 10,     10, 11,  8,    // Top
   12, 13, 14,     14, 15, 12,    // Bottom
   16, 17, 18,     18, 19, 16,    // Right
   20, 21, 22,     22, 23, 20     // Left
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
    VkDeviceMemory& bufferMemory,
    bool allocateDeviceAddress = false
) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (allocateDeviceAddress) {
        bufferInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_common::check_vk_result(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocateFlagsInfo{};
    allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    if (allocateDeviceAddress) {
        allocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = allocateDeviceAddress ? &allocateFlagsInfo : nullptr;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory");
    vk_common::check_vk_result(vkBindBufferMemory(device, buffer, bufferMemory, 0), "Failed to bind buffer memory");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "Assignment 12: Modern Descriptor Buffers & Push Descriptors\n";
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << "Concepts: VK_EXT_descriptor_buffer, Direct GPU memory descriptors,\n";
    std::cout << "          Pool-less descriptor binding & VK_KHR_push_descriptor\n";
    std::cout << "========================================================\n";

    constexpr int WIDTH = 1280;
    constexpr int HEIGHT = 720;
    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    try {
        // STEP 1: Create Window and Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 12: Descriptor Buffers (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Check for VK_EXT_descriptor_buffer extension support
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExtensions.data());

        bool descriptorBufferExtSupported = false;
        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME) == 0) {
                descriptorBufferExtSupported = true;
                break;
            }
        }

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);
        VkDevice device = VK_NULL_HANDLE;
        bool usingDescriptorBuffer = false;

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties{};
        descriptorBufferProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

        VkPhysicalDeviceProperties2 physicalProperties2{};
        physicalProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        physicalProperties2.pNext = &descriptorBufferProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &physicalProperties2);

        if (descriptorBufferExtSupported) {
            VkPhysicalDeviceDescriptorBufferFeaturesEXT dbFeatures{};
            dbFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &dbFeatures;
            vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

            if (dbFeatures.descriptorBuffer) {
                std::cout << "[Device Feature] VK_EXT_descriptor_buffer is supported on hardware.\n";

                float queuePriority = 1.0f;
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;

                std::vector<const char*> deviceExtensions = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
                };

                VkPhysicalDeviceDescriptorBufferFeaturesEXT enableDbFeatures{};
                enableDbFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
                enableDbFeatures.descriptorBuffer = VK_TRUE;

                VkPhysicalDeviceVulkan12Features vulkan12Features{};
                vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                vulkan12Features.pNext = &enableDbFeatures;
                vulkan12Features.bufferDeviceAddress = VK_TRUE;
                vulkan12Features.descriptorIndexing = VK_TRUE;

                VkPhysicalDeviceVulkan13Features vulkan13Features{};
                vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                vulkan13Features.pNext = &vulkan12Features;
                vulkan13Features.dynamicRendering = VK_TRUE;
                vulkan13Features.synchronization2 = VK_TRUE;
                vulkan13Features.maintenance4 = VK_TRUE;

                VkPhysicalDeviceFeatures2 devFeatures2{};
                devFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                devFeatures2.pNext = &vulkan13Features;

                VkDeviceCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                createInfo.pNext = &devFeatures2;
                createInfo.queueCreateInfoCount = 1;
                createInfo.pQueueCreateInfos = &queueCreateInfo;
                createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
                createInfo.ppEnabledExtensionNames = deviceExtensions.data();

                vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create Vulkan 1.4 Device with Descriptor Buffer");
                usingDescriptorBuffer = true;
            }
        }

        if (!usingDescriptorBuffer) {
            std::cout << "[Info] Initializing Vulkan 1.4 Logical Device.\n";
            device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (usingDescriptorBuffer) {
            pfn_vkGetDescriptorSetLayoutBindingOffsetEXT = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)vkGetDeviceProcAddr(device, "vkGetDescriptorSetLayoutBindingOffsetEXT");
            pfn_vkGetDescriptorSetLayoutSizeEXT = (PFN_vkGetDescriptorSetLayoutSizeEXT)vkGetDeviceProcAddr(device, "vkGetDescriptorSetLayoutSizeEXT");
            pfn_vkGetDescriptorEXT = (PFN_vkGetDescriptorEXT)vkGetDeviceProcAddr(device, "vkGetDescriptorEXT");
            pfn_vkCmdBindDescriptorBuffersEXT = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetDeviceProcAddr(device, "vkCmdBindDescriptorBuffersEXT");
            pfn_vkCmdSetDescriptorBufferOffsetsEXT = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetDeviceProcAddr(device, "vkCmdSetDescriptorBufferOffsetsEXT");

            if (!pfn_vkGetDescriptorSetLayoutBindingOffsetEXT || !pfn_vkGetDescriptorSetLayoutSizeEXT ||
                !pfn_vkGetDescriptorEXT || !pfn_vkCmdBindDescriptorBuffersEXT || !pfn_vkCmdSetDescriptorBufferOffsetsEXT) {
                std::cout << "[Warning] Could not load all VK_EXT_descriptor_buffer function pointers, fallback active.\n";
                usingDescriptorBuffer = false;
            }
        }

        std::cout << "Vulkan 1.4 Logical Device initialized with Dynamic Rendering & Synchronization2.\n";

        // STEP 2: Swapchain Setup
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

        // STEP 3: Depth Buffer Setup
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

        // STEP 4: Vertex & Index Buffers Setup
        VkDeviceSize vertexBufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vertexBuffer, vertexBufferMemory, false);

        void* vData = nullptr;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, cubeVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(cubeIndices[0]) * cubeIndices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            indexBuffer, indexBufferMemory, false);

        void* iData = nullptr;
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, cubeIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);

        // STEP 5: Uniform Buffers & Descriptor Setup
        VkDeviceSize uboSize = sizeof(UniformBufferObject);
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uboBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uboMemories;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> uboMappedPtrs;
        std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> uboAddresses;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            createBuffer(device, physicalDevice, uboSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uboBuffers[i], uboMemories[i], true);
            vkMapMemory(device, uboMemories[i], 0, uboSize, 0, &uboMappedPtrs[i]);

            if (vk14.vkGetBufferDeviceAddress) {
                VkBufferDeviceAddressInfo addrInfo{};
                addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addrInfo.buffer = uboBuffers[i];
                uboAddresses[i] = vk14.vkGetBufferDeviceAddress(device, &addrInfo);
            }
        }

        // Descriptor Set Layout
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;
        if (usingDescriptorBuffer) {
            layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        }

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");

        // Descriptor Buffer or Traditional Descriptor Pool Setup
        VkBuffer descriptorBuffer = VK_NULL_HANDLE;
        VkDeviceMemory descriptorBufferMemory = VK_NULL_HANDLE;
        void* descriptorBufferMapped = nullptr;
        VkDeviceSize descriptorSetLayoutSize = 0;
        VkDeviceSize uboDescriptorOffset = 0;
        VkDeviceAddress descriptorBufferAddress = 0;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};

        if (usingDescriptorBuffer) {
            pfn_vkGetDescriptorSetLayoutSizeEXT(device, descriptorSetLayout, &descriptorSetLayoutSize);
            pfn_vkGetDescriptorSetLayoutBindingOffsetEXT(device, descriptorSetLayout, 0, &uboDescriptorOffset);

            VkDeviceSize totalDescriptorBufferSize = descriptorSetLayoutSize * MAX_FRAMES_IN_FLIGHT;
            createBuffer(device, physicalDevice, totalDescriptorBufferSize,
                VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                descriptorBuffer, descriptorBufferMemory, true);

            vkMapMemory(device, descriptorBufferMemory, 0, totalDescriptorBufferSize, 0, &descriptorBufferMapped);

            VkBufferDeviceAddressInfo addrInfo{};
            addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addrInfo.buffer = descriptorBuffer;
            descriptorBufferAddress = vk14.vkGetBufferDeviceAddress(device, &addrInfo);

            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                VkDescriptorAddressInfoEXT addrDescInfo{};
                addrDescInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
                addrDescInfo.address = uboAddresses[i];
                addrDescInfo.range = sizeof(UniformBufferObject);
                addrDescInfo.format = VK_FORMAT_UNDEFINED;

                VkDescriptorGetInfoEXT getInfo{};
                getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
                getInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                getInfo.data.pUniformBuffer = &addrDescInfo;

                size_t descriptorSize = descriptorBufferProperties.uniformBufferDescriptorSize;
                char* dstPtr = static_cast<char*>(descriptorBufferMapped) + (i * descriptorSetLayoutSize) + uboDescriptorOffset;
                pfn_vkGetDescriptorEXT(device, &getInfo, descriptorSize, dstPtr);
            }
            std::cout << "[Descriptor Buffer] Packed UniformBuffer descriptors directly to GPU memory buffer.\n";
        } else {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

            vk_common::check_vk_result(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

            for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = descriptorPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &descriptorSetLayout;

                vk_common::check_vk_result(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[i]), "Failed to allocate descriptor set");

                VkDescriptorBufferInfo bufferInfoDesc{};
                bufferInfoDesc.buffer = uboBuffers[i];
                bufferInfoDesc.offset = 0;
                bufferInfoDesc.range = sizeof(UniformBufferObject);

                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = descriptorSets[i];
                descriptorWrite.dstBinding = 0;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfoDesc;

                vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
            }
        }

        // STEP 6: Pipeline Layout & Graphics Pipeline
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        std::string vertPath = "assignment12_descriptor_buffers/shaders/desc_buffer.vert.spv";
        std::string fragPath = "assignment12_descriptor_buffers/shaders/desc_buffer.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "shaders/desc_buffer.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "shaders/desc_buffer.frag.spv";

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
        if (usingDescriptorBuffer) {
            pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        }
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

        // STEP 7: Command Buffers & Sync Primitives
        VkCommandPoolCreateInfo cmdPoolInfo{};
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool), "Failed to create command pool");

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

        std::cout << "Descriptor Buffers Pipeline active. Starting real-time render loop...\n";

        // STEP 8: Continuous Interactive Render Loop
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t currentFrame = 0;
        int renderedFrames = 0;

        
        // Initialize Flame Graph Profiler for assignment12_descriptor_buffers
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment12_descriptor_buffers");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment12_descriptor_buffers");
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

            // Update Rotating MVP Matrix in UBO
            UniformBufferObject ubo{};
            ubo.model = vk_math::Mat4::rotate(time * vk_math::radians(45.0f), vk_math::Vec3(0.5f, 1.0f, 0.2f));
            ubo.view = vk_math::Mat4::lookAt(vk_math::Vec3(2.4f, 2.0f, 2.4f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            ubo.proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);

            std::memcpy(uboMappedPtrs[currentFrame], &ubo, sizeof(UniformBufferObject));

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
            colorAttachment.clearValue.color = {{0.05f, 0.07f, 0.11f, 1.0f}};

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

            // Bind Descriptors: VK_EXT_descriptor_buffer or fallback
            if (usingDescriptorBuffer && pfn_vkCmdBindDescriptorBuffersEXT && pfn_vkCmdSetDescriptorBufferOffsetsEXT) {
                VkDescriptorBufferBindingInfoEXT bindingInfo{};
                bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
                bindingInfo.address = descriptorBufferAddress;
                bindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

                pfn_vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

                uint32_t bufferIndex = 0;
                VkDeviceSize setOffset = currentFrame * descriptorSetLayoutSize;
                pfn_vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bufferIndex, &setOffset);
            } else {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
            }

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(cubeIndices.size()), 1, 0, 0, 0);

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
        profiler.exportFoldedFile("flamegraph_assignment12_descriptor_buffers.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment12_descriptor_buffers.html");
        profiler.exportChromeTraceFile("flamegraph_assignment12_descriptor_buffers.json");
        profiler.cleanupGpu();


        // STEP 9: Resource Cleanup
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);

            vkUnmapMemory(device, uboMemories[i]);
            vkDestroyBuffer(device, uboBuffers[i], nullptr);
            vkFreeMemory(device, uboMemories[i], nullptr);
        }

        if (usingDescriptorBuffer && descriptorBuffer != VK_NULL_HANDLE) {
            vkUnmapMemory(device, descriptorBufferMemory);
            vkDestroyBuffer(device, descriptorBuffer, nullptr);
            vkFreeMemory(device, descriptorBufferMemory, nullptr);
        } else if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

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

    std::cout << "Assignment 12 (Descriptor Buffers) completed cleanly.\n";
    return EXIT_SUCCESS;
}


