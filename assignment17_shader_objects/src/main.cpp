// ============================================================================
// Assignment 17: Next-Gen Pipeline Flexibility with Shader Objects
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_EXT_shader_object (vkCreateShadersEXT, vkDestroyShaderEXT, vkCmdBindShadersEXT)
//   - Pipeline-free rendering architecture (No VkPipeline or VkPipelineLayout)
//   - Fully dynamic graphics pipeline state (Dynamic rendering, dynamic vertex inputs,
//     dynamic culling, primitive topology, color blend, depth test, viewport/scissor)
//   - Push constant streaming across individual shader objects
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

// ----------------------------------------------------------------------------
// Vertex & Push Constant Definitions
// ----------------------------------------------------------------------------
struct Vertex2D {
    float pos[2];
    float color[3];
};

struct VertPushConstants {
    float offset[2];
    float scale;
    float angle;
};

struct FragPushConstants {
    float pad[4]; // offset to 16 bytes for matching layout(offset = 16)
    float tintColor[4];
};

struct CombinedPushConstants {
    float offset[2];
    float scale;
    float angle;
    float tintColor[4];
};

// ----------------------------------------------------------------------------
// Extension Function Pointers for VK_EXT_shader_object & Dynamic State
// ----------------------------------------------------------------------------
struct ShaderObjectDispatchTable {
    PFN_vkCreateShadersEXT vkCreateShadersEXT = nullptr;
    PFN_vkDestroyShaderEXT vkDestroyShaderEXT = nullptr;
    PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT = nullptr;
    PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT = nullptr;

    // Dynamic state functions
    PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT = nullptr;
    PFN_vkCmdSetCullMode vkCmdSetCullMode = nullptr;
    PFN_vkCmdSetFrontFace vkCmdSetFrontFace = nullptr;
    PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology = nullptr;
    PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount = nullptr;
    PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount = nullptr;
    PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable = nullptr;
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT = nullptr;
    PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT = nullptr;
    PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT = nullptr;
    PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT = nullptr;
    PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable = nullptr;
    PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable = nullptr;
    PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable = nullptr;
    PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable = nullptr;
    PFN_vkCmdSetStencilOp vkCmdSetStencilOp = nullptr;
    PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT = nullptr;
    PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT = nullptr;
    PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT = nullptr;
    PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable = nullptr;

    bool load(VkInstance instance, VkDevice device) {
        vkCreateShadersEXT = (PFN_vkCreateShadersEXT)vkGetDeviceProcAddr(device, "vkCreateShadersEXT");
        vkDestroyShaderEXT = (PFN_vkDestroyShaderEXT)vkGetDeviceProcAddr(device, "vkDestroyShaderEXT");
        vkGetShaderBinaryDataEXT = (PFN_vkGetShaderBinaryDataEXT)vkGetDeviceProcAddr(device, "vkGetShaderBinaryDataEXT");
        vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)vkGetDeviceProcAddr(device, "vkCmdBindShadersEXT");

        vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)vkGetDeviceProcAddr(device, "vkCmdSetVertexInputEXT");
        if (!vkCmdSetVertexInputEXT) {
            vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)vkGetInstanceProcAddr(instance, "vkCmdSetVertexInputEXT");
        }

        vkCmdSetCullMode = (PFN_vkCmdSetCullMode)vkGetDeviceProcAddr(device, "vkCmdSetCullMode");
        if (!vkCmdSetCullMode) vkCmdSetCullMode = (PFN_vkCmdSetCullMode)vkGetDeviceProcAddr(device, "vkCmdSetCullModeEXT");

        vkCmdSetFrontFace = (PFN_vkCmdSetFrontFace)vkGetDeviceProcAddr(device, "vkCmdSetFrontFace");
        if (!vkCmdSetFrontFace) vkCmdSetFrontFace = (PFN_vkCmdSetFrontFace)vkGetDeviceProcAddr(device, "vkCmdSetFrontFaceEXT");

        vkCmdSetPrimitiveTopology = (PFN_vkCmdSetPrimitiveTopology)vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveTopology");
        if (!vkCmdSetPrimitiveTopology) vkCmdSetPrimitiveTopology = (PFN_vkCmdSetPrimitiveTopology)vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveTopologyEXT");

        vkCmdSetViewportWithCount = (PFN_vkCmdSetViewportWithCount)vkGetDeviceProcAddr(device, "vkCmdSetViewportWithCount");
        if (!vkCmdSetViewportWithCount) vkCmdSetViewportWithCount = (PFN_vkCmdSetViewportWithCount)vkGetDeviceProcAddr(device, "vkCmdSetViewportWithCountEXT");

        vkCmdSetScissorWithCount = (PFN_vkCmdSetScissorWithCount)vkGetDeviceProcAddr(device, "vkCmdSetScissorWithCount");
        if (!vkCmdSetScissorWithCount) vkCmdSetScissorWithCount = (PFN_vkCmdSetScissorWithCount)vkGetDeviceProcAddr(device, "vkCmdSetScissorWithCountEXT");

        vkCmdSetRasterizerDiscardEnable = (PFN_vkCmdSetRasterizerDiscardEnable)vkGetDeviceProcAddr(device, "vkCmdSetRasterizerDiscardEnable");
        if (!vkCmdSetRasterizerDiscardEnable) vkCmdSetRasterizerDiscardEnable = (PFN_vkCmdSetRasterizerDiscardEnable)vkGetDeviceProcAddr(device, "vkCmdSetRasterizerDiscardEnableEXT");

        vkCmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)vkGetDeviceProcAddr(device, "vkCmdSetPolygonModeEXT");
        vkCmdSetRasterizationSamplesEXT = (PFN_vkCmdSetRasterizationSamplesEXT)vkGetDeviceProcAddr(device, "vkCmdSetRasterizationSamplesEXT");
        vkCmdSetSampleMaskEXT = (PFN_vkCmdSetSampleMaskEXT)vkGetDeviceProcAddr(device, "vkCmdSetSampleMaskEXT");
        vkCmdSetAlphaToCoverageEnableEXT = (PFN_vkCmdSetAlphaToCoverageEnableEXT)vkGetDeviceProcAddr(device, "vkCmdSetAlphaToCoverageEnableEXT");

        vkCmdSetDepthTestEnable = (PFN_vkCmdSetDepthTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnable");
        if (!vkCmdSetDepthTestEnable) vkCmdSetDepthTestEnable = (PFN_vkCmdSetDepthTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT");

        vkCmdSetDepthWriteEnable = (PFN_vkCmdSetDepthWriteEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnable");
        if (!vkCmdSetDepthWriteEnable) vkCmdSetDepthWriteEnable = (PFN_vkCmdSetDepthWriteEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnableEXT");

        vkCmdSetDepthCompareOp = (PFN_vkCmdSetDepthCompareOp)vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOp");
        if (!vkCmdSetDepthCompareOp) vkCmdSetDepthCompareOp = (PFN_vkCmdSetDepthCompareOp)vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOpEXT");

        vkCmdSetDepthBoundsTestEnable = (PFN_vkCmdSetDepthBoundsTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthBoundsTestEnable");
        if (!vkCmdSetDepthBoundsTestEnable) vkCmdSetDepthBoundsTestEnable = (PFN_vkCmdSetDepthBoundsTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetDepthBoundsTestEnableEXT");

        vkCmdSetStencilTestEnable = (PFN_vkCmdSetStencilTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetStencilTestEnable");
        if (!vkCmdSetStencilTestEnable) vkCmdSetStencilTestEnable = (PFN_vkCmdSetStencilTestEnable)vkGetDeviceProcAddr(device, "vkCmdSetStencilTestEnableEXT");

        vkCmdSetStencilOp = (PFN_vkCmdSetStencilOp)vkGetDeviceProcAddr(device, "vkCmdSetStencilOp");
        if (!vkCmdSetStencilOp) vkCmdSetStencilOp = (PFN_vkCmdSetStencilOp)vkGetDeviceProcAddr(device, "vkCmdSetStencilOpEXT");

        vkCmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT");
        vkCmdSetColorBlendEquationEXT = (PFN_vkCmdSetColorBlendEquationEXT)vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT");
        vkCmdSetColorWriteMaskEXT = (PFN_vkCmdSetColorWriteMaskEXT)vkGetDeviceProcAddr(device, "vkCmdSetColorWriteMaskEXT");

        vkCmdSetPrimitiveRestartEnable = (PFN_vkCmdSetPrimitiveRestartEnable)vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveRestartEnable");
        if (!vkCmdSetPrimitiveRestartEnable) vkCmdSetPrimitiveRestartEnable = (PFN_vkCmdSetPrimitiveRestartEnable)vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveRestartEnableEXT");

        return vkCreateShadersEXT && vkDestroyShaderEXT && vkCmdBindShadersEXT;
    }
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

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 17: Shader Objects (VK_EXT_shader_object)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Pipeline-Free Rendering, Dynamic State Binding,\n";
    std::cout << "           vkCreateShadersEXT, vkCmdBindShadersEXT, Push Constants\n";
    std::cout << "========================================================\n";

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    try {
        // STEP 1: Window & Instance Creation
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 17: Shader Objects (VK_EXT_shader_object)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // STEP 2: Check Physical Device Feature Support for Shader Objects
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExts(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExts.data());

        bool shaderObjectExtSupported = false;
        for (const auto& ext : availableExts) {
            if (strcmp(ext.extensionName, VK_EXT_SHADER_OBJECT_EXTENSION_NAME) == 0) {
                shaderObjectExtSupported = true;
                break;
            }
        }

        if (!shaderObjectExtSupported) {
            std::cerr << "[Warning] VK_EXT_shader_object is not supported on this physical device.\n";
            std::cerr << "[Fallback] Demonstrating Vulkan 1.4 Dynamic Rendering fallback with pipeline architecture.\n";
        }

        // STEP 3: Find Graphics Queue Family
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

        // STEP 4: Logical Device Creation with VK_EXT_shader_object (if supported) & Vulkan 1.4 core features
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        if (shaderObjectExtSupported) {
            deviceExtensions.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
        }

        VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{};
        shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
        shaderObjectFeatures.shaderObject = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;
        vulkan13Features.maintenance4 = VK_TRUE;
        vulkan13Features.pNext = shaderObjectExtSupported ? (void*)&shaderObjectFeatures : nullptr;

        VkPhysicalDeviceVulkan14Features vulkan14Features{};
        vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vulkan14Features.pNext = &vulkan13Features;
        vulkan14Features.vertexAttributeInstanceRateDivisor = VK_TRUE;
        vulkan14Features.vertexAttributeInstanceRateZeroDivisor = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan14Features;
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
        deviceFeatures2.features.multiDrawIndirect = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &deviceFeatures2;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
        bool usingShaderObjects = false;

        if (res != VK_SUCCESS && shaderObjectExtSupported) {
            // Fallback without extension
            std::cerr << "[Warning] Device creation with VK_EXT_shader_object failed, retrying standard Vulkan 1.4 core.\n";
            deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            vulkan13Features.pNext = nullptr;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();
            vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create fallback Vulkan device");
            usingShaderObjects = false;
        } else if (res == VK_SUCCESS && shaderObjectExtSupported) {
            usingShaderObjects = true;
        } else {
            vk_common::check_vk_result(res, "Failed to create Vulkan logical device");
            usingShaderObjects = false;
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        ShaderObjectDispatchTable shaderObjTable{};
        if (usingShaderObjects) {
            if (!shaderObjTable.load(instance, device)) {
                std::cerr << "[Warning] Failed to load all VK_EXT_shader_object function pointers. Falling back.\n";
                usingShaderObjects = false;
            }
        }

        std::cout << "[Pipeline Mode] " << (usingShaderObjects ? "VK_EXT_shader_object (PIPELINE-FREE RENDERING ENABLED)" : "Vulkan 1.4 Dynamic Rendering Pipeline") << "\n";

        // STEP 5: Create Swapchain
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

            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create image view");
        }

        // STEP 6: Vertex & Index Buffer Generation (Hexagon Fan & Dynamic Star Geometries)
        std::vector<Vertex2D> vertices = {
            // Hexagon Center & Outer Ring
            {{ 0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}}, // Center (0)
            {{ 0.0f, -0.6f}, {1.0f, 0.2f, 0.2f}}, // Top (1)
            {{ 0.52f, -0.3f}, {1.0f, 0.8f, 0.2f}}, // Top-Right (2)
            {{ 0.52f,  0.3f}, {0.2f, 1.0f, 0.3f}}, // Bottom-Right (3)
            {{ 0.0f,  0.6f}, {0.2f, 0.9f, 1.0f}}, // Bottom (4)
            {{-0.52f,  0.3f}, {0.6f, 0.2f, 1.0f}}, // Bottom-Left (5)
            {{-0.52f, -0.3f}, {1.0f, 0.2f, 0.8f}}  // Top-Left (6)
        };

        std::vector<uint16_t> indices = {
            0, 1, 2,
            0, 2, 3,
            0, 3, 4,
            0, 4, 5,
            0, 5, 6,
            0, 6, 1
        };

        VkBuffer vertexBuffer, indexBuffer;
        VkDeviceMemory vertexBufferMemory, indexBufferMemory;

        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);

        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        memcpy(vData, vertices.data(), vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     indexBuffer, indexBufferMemory);

        void* iData;
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &iData);
        memcpy(iData, indices.data(), indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);

        // STEP 7: Load SPIR-V Shaders
        std::string vertPath = "shaders/shader_obj.vert.spv";
        std::string fragPath = "shaders/shader_obj.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment17_shader_objects/shaders/shader_obj.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment17_shader_objects/shaders/shader_obj.frag.spv";

        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

        // Create Push Constant Range
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(CombinedPushConstants);

        VkShaderEXT vertShader = VK_NULL_HANDLE;
        VkShaderEXT fragShader = VK_NULL_HANDLE;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkShaderModule vertModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;

        if (usingShaderObjects) {
            // STEP 8A: Pipeline-free Shader Object Creation via vkCreateShadersEXT
            VkShaderCreateInfoEXT shaderCreateInfos[2]{};

            // Vertex Shader Object
            shaderCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
            shaderCreateInfos[0].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
            shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderCreateInfos[0].nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderCreateInfos[0].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
            shaderCreateInfos[0].codeSize = vertCode.size();
            shaderCreateInfos[0].pCode = vertCode.data();
            shaderCreateInfos[0].pName = "main";
            shaderCreateInfos[0].setLayoutCount = 0;
            shaderCreateInfos[0].pSetLayouts = nullptr;
            shaderCreateInfos[0].pushConstantRangeCount = 1;
            shaderCreateInfos[0].pPushConstantRanges = &pushConstantRange;

            // Fragment Shader Object
            shaderCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
            shaderCreateInfos[1].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
            shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderCreateInfos[1].nextStage = 0;
            shaderCreateInfos[1].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
            shaderCreateInfos[1].codeSize = fragCode.size();
            shaderCreateInfos[1].pCode = fragCode.data();
            shaderCreateInfos[1].pName = "main";
            shaderCreateInfos[1].setLayoutCount = 0;
            shaderCreateInfos[1].pSetLayouts = nullptr;
            shaderCreateInfos[1].pushConstantRangeCount = 1;
            shaderCreateInfos[1].pPushConstantRanges = &pushConstantRange;

            VkShaderEXT shaders[2];
            vk_common::check_vk_result(
                shaderObjTable.vkCreateShadersEXT(device, 2, shaderCreateInfos, nullptr, shaders),
                "Failed to create Shader Objects via vkCreateShadersEXT"
            );

            vertShader = shaders[0];
            fragShader = shaders[1];
            std::cout << "[VK_EXT_shader_object] Successfully created standalone Vertex and Fragment Shader Objects!\n";
        } else {
            // STEP 8B: Fallback Pipeline Layout & Standard Graphics Pipeline
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

            vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

            vertModule = vulkan_utils::createShaderModule(device, vertCode);
            fragModule = vulkan_utils::createShaderModule(device, fragCode);

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

            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex2D);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex2D, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex2D, color);

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
            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

            VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
            pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            pipelineRenderingCreateInfo.colorAttachmentCount = 1;
            pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

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
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = VK_NULL_HANDLE;

            vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline), "Failed to create fallback pipeline");
        }

        // STEP 9: Command Pool & Frame Synchronization Setup
        const int MAX_FRAMES_IN_FLIGHT = 2;

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()), "Failed to allocate command buffers");

        std::vector<VkSemaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkSemaphore> renderFinishedSemaphores(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]), "Failed to create semaphore");
            vk_common::check_vk_result(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]), "Failed to create semaphore");
            vk_common::check_vk_result(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]), "Failed to create fence");
        }

        std::cout << "[Render Loop] Entering Real-Time Shader Object Render Loop...\n";

        // Dynamic State Specification structures for vkCmdSetVertexInputEXT
        VkVertexInputBindingDescription2EXT bindingDesc2{};
        bindingDesc2.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
        bindingDesc2.binding = 0;
        bindingDesc2.stride = sizeof(Vertex2D);
        bindingDesc2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDesc2.divisor = 1;

        VkVertexInputAttributeDescription2EXT attributeDesc2[2]{};
        attributeDesc2[0].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
        attributeDesc2[0].location = 0;
        attributeDesc2[0].binding = 0;
        attributeDesc2[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDesc2[0].offset = offsetof(Vertex2D, pos);

        attributeDesc2[1].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
        attributeDesc2[1].location = 1;
        attributeDesc2[1].binding = 0;
        attributeDesc2[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDesc2[1].offset = offsetof(Vertex2D, color);

        VkColorBlendEquationEXT blendEquation{};
        blendEquation.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendEquation.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendEquation.colorBlendOp = VK_BLEND_OP_ADD;
        blendEquation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendEquation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendEquation.alphaBlendOp = VK_BLEND_OP_ADD;

        VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkBool32 colorBlendEnable = VK_FALSE;

        uint32_t currentFrame = 0;
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t renderedFrames = 0;

        // STEP 10: Render Loop (Dynamic Multi-Object Push Constant Rendering)
        
        // Initialize Flame Graph Profiler for assignment17_shader_objects
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment17_shader_objects");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment17_shader_objects");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFences[currentFrame]);

            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                                         imageAvailableSemaphores[currentFrame],
                                                         VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            VkCommandBuffer cmd = commandBuffers[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &beginInfo);

            // Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                cmd,
                vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
            );

            // Dynamic Rendering Begin
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.05f, 0.07f, 0.12f, 1.0f}};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vk14.vkCmdBeginRendering(cmd, &renderingInfo);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};

            if (usingShaderObjects) {
                // Bind Shader Objects dynamically (No pipeline bound!)
                VkShaderStageFlagBits stages[2] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
                VkShaderEXT shaderObjs[2] = { vertShader, fragShader };
                shaderObjTable.vkCmdBindShadersEXT(cmd, 2, stages, shaderObjs);

                // Set dynamic state for the pipeline-free state vector
                if (shaderObjTable.vkCmdSetVertexInputEXT) {
                    shaderObjTable.vkCmdSetVertexInputEXT(cmd, 1, &bindingDesc2, 2, attributeDesc2);
                }
                if (shaderObjTable.vkCmdSetPrimitiveTopology) {
                    shaderObjTable.vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
                }
                if (shaderObjTable.vkCmdSetPrimitiveRestartEnable) {
                    shaderObjTable.vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetViewportWithCount) {
                    shaderObjTable.vkCmdSetViewportWithCount(cmd, 1, &viewport);
                } else {
                    vkCmdSetViewport(cmd, 0, 1, &viewport);
                }
                if (shaderObjTable.vkCmdSetScissorWithCount) {
                    shaderObjTable.vkCmdSetScissorWithCount(cmd, 1, &scissor);
                } else {
                    vkCmdSetScissor(cmd, 0, 1, &scissor);
                }
                if (shaderObjTable.vkCmdSetRasterizerDiscardEnable) {
                    shaderObjTable.vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetCullMode) {
                    shaderObjTable.vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
                }
                if (shaderObjTable.vkCmdSetFrontFace) {
                    shaderObjTable.vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);
                }
                if (shaderObjTable.vkCmdSetPolygonModeEXT) {
                    shaderObjTable.vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
                }
                if (shaderObjTable.vkCmdSetRasterizationSamplesEXT) {
                    shaderObjTable.vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
                }
                VkSampleMask sampleMask = 0xFFFFFFFF;
                if (shaderObjTable.vkCmdSetSampleMaskEXT) {
                    shaderObjTable.vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
                }
                if (shaderObjTable.vkCmdSetAlphaToCoverageEnableEXT) {
                    shaderObjTable.vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetDepthTestEnable) {
                    shaderObjTable.vkCmdSetDepthTestEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetDepthWriteEnable) {
                    shaderObjTable.vkCmdSetDepthWriteEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetDepthBoundsTestEnable) {
                    shaderObjTable.vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetStencilTestEnable) {
                    shaderObjTable.vkCmdSetStencilTestEnable(cmd, VK_FALSE);
                }
                if (shaderObjTable.vkCmdSetColorBlendEnableEXT) {
                    shaderObjTable.vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &colorBlendEnable);
                }
                if (shaderObjTable.vkCmdSetColorBlendEquationEXT) {
                    shaderObjTable.vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendEquation);
                }
                if (shaderObjTable.vkCmdSetColorWriteMaskEXT) {
                    shaderObjTable.vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &colorWriteMask);
                }
            } else {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
                vkCmdSetViewport(cmd, 0, 1, &viewport);
                vkCmdSetScissor(cmd, 0, 1, &scissor);
            }

            // Bind Geometry
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Draw Multiple Objects with Varied Push Constants (Dynamic Scaling, Rotation & Tinting)
            const int NUM_SHAPES = 6;
            for (int obj = 0; obj < NUM_SHAPES; ++obj) {
                float angleOffset = (2.0f * 3.14159265f / NUM_SHAPES) * obj;
                float currentAngle = time * (obj % 2 == 0 ? 1.2f : -0.8f) + angleOffset;
                float radius = 0.55f;

                CombinedPushConstants pushData{};
                pushData.offset[0] = std::cos(angleOffset + time * 0.5f) * radius;
                pushData.offset[1] = std::sin(angleOffset + time * 0.5f) * radius;
                pushData.scale = 0.35f + 0.1f * std::sin(time * 2.0f + obj);
                pushData.angle = currentAngle;

                // Unique tint color per object
                pushData.tintColor[0] = 0.5f + 0.5f * std::sin(time + obj * 1.0f);
                pushData.tintColor[1] = 0.5f + 0.5f * std::sin(time + obj * 2.0f + 2.0f);
                pushData.tintColor[2] = 0.5f + 0.5f * std::sin(time + obj * 3.0f + 4.0f);
                pushData.tintColor[3] = 1.0f;

                if (usingShaderObjects) {
                    // Push constants directly to bound shader stages without pipeline layout
                    // Passing nullptr / VK_NULL_HANDLE layout when using VK_EXT_shader_object or using a dummy layout is standard
                    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(CombinedPushConstants), &pushData);
                } else {
                    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(CombinedPushConstants), &pushData);
                }

                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            }

            // End Dynamic Rendering
            vk14.vkCmdEndRendering(cmd);

            // Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
            vulkan_utils::pipelineBarrier2ImageTransition(
                cmd,
                vk14.vkCmdPipelineBarrier2,
                swapchainImages[imageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_2_NONE
            );

            vkEndCommandBuffer(cmd);

            // Submit
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

            // Present
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

            if (renderedFrames % 60 == 0) {
                std::cout << "[Frame #" << renderedFrames << "] Pipeline-free Shader Objects actively animating " << NUM_SHAPES << " objects." << std::endl;
            }

            // If running in automated non-interactive / verification mode, allow clean completion after initial batch of frames
            const char* autoTest = std::getenv("VULKAN_AUTO_TEST");
            if (autoTest && renderedFrames >= 180) {
                std::cout << "[Auto-Test] Reached " << renderedFrames << " frames in automated verification mode. Exiting loop cleanly." << std::endl;
                break;
            }
        }

        std::cout << "[Status] Completed rendering " << renderedFrames << " frames." << std::endl;

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment17_shader_objects.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment17_shader_objects.html");
        profiler.exportChromeTraceFile("flamegraph_assignment17_shader_objects.json");
        profiler.cleanupGpu();


        // STEP 11: Cleanup
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        if (usingShaderObjects) {
            if (vertShader != VK_NULL_HANDLE) shaderObjTable.vkDestroyShaderEXT(device, vertShader, nullptr);
            if (fragShader != VK_NULL_HANDLE) shaderObjTable.vkDestroyShaderEXT(device, fragShader, nullptr);
        }

        if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, graphicsPipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, fragModule, nullptr);

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

    std::cout << "Assignment 17 (Shader Objects) completed cleanly.\n";
    return EXIT_SUCCESS;
}
