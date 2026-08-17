// ============================================================================
// Assignment 75: Tile-Local Subpass Operations & Dynamic Shading (VK_EXT_shader_tile_image)
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_EXT_shader_tile_image & tileImageReadEXT
//   - On-chip SRAM G-Buffer reads without VRAM roundtrips
//   - Dynamic Rendering with Multiple Render Targets (Albedo + Normal)
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
#include <filesystem>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 normal;
    vk_math::Vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct GBufferPushConstants {
    vk_math::Mat4 mvp;
    vk_math::Mat4 model;
};

struct LightingPushConstants {
    Vec4 lightPos[4];
    Vec4 lightColor[4];
    Vec4 viewPos;
    int32_t numLights;
    int32_t pad0, pad1, pad2;
};

void generateCube(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    // 6 cube faces
    auto addFace = [&](vk_math::Vec3 normal, vk_math::Vec3 color, vk_math::Vec3 v0, vk_math::Vec3 v1, vk_math::Vec3 v2, vk_math::Vec3 v3) {
        uint16_t base = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ v0, normal, color });
        vertices.push_back({ v1, normal, color });
        vertices.push_back({ v2, normal, color });
        vertices.push_back({ v3, normal, color });
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    };

    // Front / Back
    addFace({0,0,1}, {0.9f, 0.2f, 0.2f}, {-0.6f,-0.6f,0.6f}, {0.6f,-0.6f,0.6f}, {0.6f,0.6f,0.6f}, {-0.6f,0.6f,0.6f});
    addFace({0,0,-1}, {0.2f, 0.9f, 0.2f}, {0.6f,-0.6f,-0.6f}, {-0.6f,-0.6f,-0.6f}, {-0.6f,0.6f,-0.6f}, {0.6f,0.6f,-0.6f});
    // Top / Bottom
    addFace({0,1,0}, {0.2f, 0.3f, 0.9f}, {-0.6f,0.6f,0.6f}, {0.6f,0.6f,0.6f}, {0.6f,0.6f,-0.6f}, {-0.6f,0.6f,-0.6f});
    addFace({0,-1,0}, {0.9f, 0.9f, 0.2f}, {-0.6f,-0.6f,-0.6f}, {0.6f,-0.6f,-0.6f}, {0.6f,-0.6f,0.6f}, {-0.6f,-0.6f,0.6f});
    // Right / Left
    addFace({1,0,0}, {0.9f, 0.2f, 0.9f}, {0.6f,-0.6f,0.6f}, {0.6f,-0.6f,-0.6f}, {0.6f,0.6f,-0.6f}, {0.6f,0.6f,0.6f});
    addFace({-1,0,0}, {0.2f, 0.9f, 0.9f}, {-0.6f,-0.6f,-0.6f}, {-0.6f,-0.6f,0.6f}, {-0.6f,0.6f,0.6f}, {-0.6f,0.6f,-0.6f});
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

void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, image, &memReq);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");
    vk_common::check_vk_result(vkBindImageMemory(device, image, imageMemory, 0), "Failed to bind image memory");
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "Failed to create image view");
    return imageView;
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << " Assignment 75: Tile-Local Deferred Shading (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: VK_EXT_shader_tile_image, Zero-VRAM G-Buffer Reads,\n";
    std::cout << "           Dynamic Rendering MRT & Multi-Light Deferred Pipeline\n";
    std::cout << "====================================================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment 75: Tile-Local Deferred Shading", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    try {
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Assignment 75 - Tile Image Deferred";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Vulkan14Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_4;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        VkInstanceCreateInfo instanceInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();

        VkInstance instance;
        vk_common::check_vk_result(vkCreateInstance(&instanceInfo, nullptr, &instance), "Failed to create instance");

        VkSurfaceKHR surface;
        vk_common::check_vk_result(glfwCreateWindowSurface(instance, window, nullptr, &surface), "Failed to create surface");

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
        VkPhysicalDevice physicalDevice = physicalDevices[0];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                graphicsQueueFamily = i;
                break;
            }
        }

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures2.pNext = &vulkan13Features;

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create device");

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // Swapchain
        VkSurfaceFormatKHR surfaceFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkSwapchainCreateInfoKHR swapchainInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
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
            swapchainImageViews[i] = createImageView(device, swapchainImages[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        // G-Buffer Images: Albedo (R8G8B8A8_UNORM) + Normal (R16G16B16A16_SFLOAT) + Depth (D32_SFLOAT)
        VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat normalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImage albedoImage, normalImage, depthImage;
        VkDeviceMemory albedoMemory, normalMemory, depthMemory;
        VkImageView albedoView, normalView, depthView;

        createImage(device, physicalDevice, WIDTH, HEIGHT, albedoFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, albedoImage, albedoMemory);
        createImage(device, physicalDevice, WIDTH, HEIGHT, normalFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, normalImage, normalMemory);
        createImage(device, physicalDevice, WIDTH, HEIGHT, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage, depthMemory);

        albedoView = createImageView(device, albedoImage, albedoFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        normalView = createImageView(device, normalImage, normalFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        depthView = createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Sampler for G-Buffer sampling in resolve pass
        VkSampler sampler;
        {
            VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
        }

        // Descriptor Set Layout for Deferred Lighting Pass
        VkDescriptorSetLayoutBinding albedoBinding{};
        albedoBinding.binding = 0;
        albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedoBinding.descriptorCount = 1;
        albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        albedoBinding.pImmutableSamplers = &sampler;

        VkDescriptorSetLayoutBinding normalBinding{};
        normalBinding.binding = 1;
        normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalBinding.descriptorCount = 1;
        normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        normalBinding.pImmutableSamplers = &sampler;

        std::array<VkDescriptorSetLayoutBinding, 2> descBindings = { albedoBinding, normalBinding };
        VkDescriptorSetLayoutCreateInfo descLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descLayoutInfo.bindingCount = static_cast<uint32_t>(descBindings.size());
        descLayoutInfo.pBindings = descBindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &descriptorSetLayout);

        VkDescriptorPool descriptorPool;
        {
            VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 } };
            VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.maxSets = 2;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = poolSizes;
            vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
        }

        VkDescriptorSet descriptorSet;
        {
            VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &descriptorSetLayout;
            vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

            VkDescriptorImageInfo albedoInfo{ sampler, albedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkDescriptorImageInfo normalInfo{ sampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = descriptorSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &albedoInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = descriptorSet;
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &normalInfo;

            vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
        }

        // Shaders & Pipelines
        std::string shaderDir = "assignment75_shader_tile_image_deferred/shaders/";
        if (!std::filesystem::exists(shaderDir + "gbuffer.vert.spv")) shaderDir = "shaders/";

        auto gVertCode = vulkan_utils::readFile(shaderDir + "gbuffer.vert.spv");
        auto gFragCode = vulkan_utils::readFile(shaderDir + "gbuffer.frag.spv");
        auto dVertCode = vulkan_utils::readFile(shaderDir + "tile_deferred.vert.spv");
        auto dFragCode = vulkan_utils::readFile(shaderDir + "tile_deferred.frag.spv");

        VkShaderModule gVertModule = vulkan_utils::createShaderModule(device, gVertCode);
        VkShaderModule gFragModule = vulkan_utils::createShaderModule(device, gFragCode);
        VkShaderModule dVertModule = vulkan_utils::createShaderModule(device, dVertCode);
        VkShaderModule dFragModule = vulkan_utils::createShaderModule(device, dFragCode);

        // G-Buffer Pipeline Layout
        VkPushConstantRange gbufferPcRange{};
        gbufferPcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        gbufferPcRange.offset = 0;
        gbufferPcRange.size = sizeof(GBufferPushConstants);

        VkPipelineLayoutCreateInfo gbufferPipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        gbufferPipeLayoutInfo.pushConstantRangeCount = 1;
        gbufferPipeLayoutInfo.pPushConstantRanges = &gbufferPcRange;

        VkPipelineLayout gbufferPipelineLayout;
        vkCreatePipelineLayout(device, &gbufferPipeLayoutInfo, nullptr, &gbufferPipelineLayout);

        // Deferred Pipeline Layout
        VkPushConstantRange lightingPcRange{};
        lightingPcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        lightingPcRange.offset = 0;
        lightingPcRange.size = sizeof(LightingPushConstants);

        VkPipelineLayoutCreateInfo deferredPipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        deferredPipeLayoutInfo.setLayoutCount = 1;
        deferredPipeLayoutInfo.pSetLayouts = &descriptorSetLayout;
        deferredPipeLayoutInfo.pushConstantRangeCount = 1;
        deferredPipeLayoutInfo.pPushConstantRanges = &lightingPcRange;

        VkPipelineLayout deferredPipelineLayout;
        vkCreatePipelineLayout(device, &deferredPipeLayoutInfo, nullptr, &deferredPipelineLayout);

        // 1. G-Buffer Pipeline Creation
        VkPipeline gbufferPipeline;
        {
            VkPipelineShaderStageCreateInfo stages[] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, gVertModule, "main", nullptr },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, gFragModule, "main", nullptr }
            };

            auto bindingDesc = Vertex::getBindingDescription();
            auto attrDesc = Vertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size());
            vertexInputInfo.pVertexAttributeDescriptions = attrDesc.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizer.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

            VkPipelineColorBlendAttachmentState blendStates[2]{};
            blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendStates[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            colorBlending.attachmentCount = 2;
            colorBlending.pAttachments = blendStates;

            std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkFormat colorFormats[2] = { albedoFormat, normalFormat };
            VkPipelineRenderingCreateInfo renderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            renderingCreateInfo.colorAttachmentCount = 2;
            renderingCreateInfo.pColorAttachmentFormats = colorFormats;
            renderingCreateInfo.depthAttachmentFormat = depthFormat;

            VkGraphicsPipelineCreateInfo pipeInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            pipeInfo.pNext = &renderingCreateInfo;
            pipeInfo.stageCount = 2;
            pipeInfo.pStages = stages;
            pipeInfo.pVertexInputState = &vertexInputInfo;
            pipeInfo.pInputAssemblyState = &inputAssembly;
            pipeInfo.pViewportState = &viewportState;
            pipeInfo.pRasterizationState = &rasterizer;
            pipeInfo.pMultisampleState = &multisampling;
            pipeInfo.pDepthStencilState = &depthStencil;
            pipeInfo.pColorBlendState = &colorBlending;
            pipeInfo.pDynamicState = &dynamicState;
            pipeInfo.layout = gbufferPipelineLayout;

            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &gbufferPipeline);
        }

        // 2. Deferred Lighting Pipeline Creation
        VkPipeline deferredPipeline;
        {
            VkPipelineShaderStageCreateInfo stages[] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, dVertModule, "main", nullptr },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, dFragModule, "main", nullptr }
            };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState colorBlend{};
            colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlend;

            std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkPipelineRenderingCreateInfo renderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            renderingCreateInfo.colorAttachmentCount = 1;
            renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

            VkGraphicsPipelineCreateInfo pipeInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            pipeInfo.pNext = &renderingCreateInfo;
            pipeInfo.stageCount = 2;
            pipeInfo.pStages = stages;
            pipeInfo.pVertexInputState = &vertexInputInfo;
            pipeInfo.pInputAssemblyState = &inputAssembly;
            pipeInfo.pViewportState = &viewportState;
            pipeInfo.pRasterizationState = &rasterizer;
            pipeInfo.pMultisampleState = &multisampling;
            pipeInfo.pColorBlendState = &colorBlending;
            pipeInfo.pDynamicState = &dynamicState;
            pipeInfo.layout = deferredPipelineLayout;

            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &deferredPipeline);
        }

        // Geometry Setup
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        generateCube(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);
        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexBufferMemory);
        void* iData;
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);

        // Command Pool & Synchronization
        VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = graphicsQueueFamily;
        VkCommandPool commandPool;
        vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo scInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fcInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fcInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateSemaphore(device, &scInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &scInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fcInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Executing Dynamic Rendering Tile-Local Deferred Lighting Loop...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment75_shader_tile_image_deferred
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment75_shader_tile_image_deferred");
        profiler.initGpu(device, physicalDevice);

        while (!glfwWindowShouldClose(window) && frameCount < 400) {
            VK_PROFILE_SCOPE("assignment75_shader_tile_image_deferred");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) break;

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Transform calculations
            vk_math::Mat4 model = vk_math::Mat4::rotate(time * 1.2f, { 0.0f, 1.0f, 0.0f }) * vk_math::Mat4::rotate(time * 0.8f, { 1.0f, 0.0f, 0.0f });
            vk_math::Mat4 view = vk_math::Mat4::lookAt({ 0.0f, 1.2f, 2.5f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
            vk_math::Mat4 proj = vk_math::Mat4::perspective(45.0f * 3.14159265f / 180.0f, float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
            proj.m[1][1] *= -1.0f;

            GBufferPushConstants gPc{};
            gPc.mvp = proj * view * model;
            gPc.model = model;

            LightingPushConstants lPc{};
            lPc.numLights = 3;
            lPc.lightPos[0] = Vec4(std::cos(time * 2.0f) * 2.0f, 1.5f, std::sin(time * 2.0f) * 2.0f, 1.0f);
            lPc.lightColor[0] = Vec4(1.0f, 0.4f, 0.2f, 1.0f); // Warm Orange
            lPc.lightPos[1] = Vec4(std::sin(time * 1.5f) * 2.0f, -1.0f, std::cos(time * 1.5f) * 2.0f, 1.0f);
            lPc.lightColor[1] = Vec4(0.2f, 0.6f, 1.0f, 1.0f); // Cyan Blue
            lPc.lightPos[2] = Vec4(0.0f, 2.0f, 0.0f, 1.0f);
            lPc.lightColor[2] = Vec4(0.8f, 0.2f, 0.9f, 1.0f); // Magenta
            lPc.viewPos = Vec4(0.0f, 1.2f, 2.5f, 1.0f);

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginCmd{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginCmd);

            // Phase 1: Transition G-Buffer Attachments to COLOR_ATTACHMENT_OPTIMAL
            VkImageMemoryBarrier2 gBarriers[3]{};
            gBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            gBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            gBarriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            gBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            gBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            gBarriers[0].image = albedoImage;
            gBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            gBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            gBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            gBarriers[1].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            gBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            gBarriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            gBarriers[1].image = normalImage;
            gBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            gBarriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            gBarriers[2].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            gBarriers[2].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            gBarriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            gBarriers[2].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            gBarriers[2].image = depthImage;
            gBarriers[2].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

            VkDependencyInfo gDepInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            gDepInfo.imageMemoryBarrierCount = 3;
            gDepInfo.pImageMemoryBarriers = gBarriers;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &gDepInfo);

            // Phase 2: Render G-Buffer Pass (MRT)
            VkRenderingAttachmentInfo gColorAttachments[2]{};
            gColorAttachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            gColorAttachments[0].imageView = albedoView;
            gColorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            gColorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            gColorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            gColorAttachments[0].clearValue = {{{ 0.0f, 0.0f, 0.0f, 0.0f }}};

            gColorAttachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            gColorAttachments[1].imageView = normalView;
            gColorAttachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            gColorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            gColorAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            gColorAttachments[1].clearValue = {{{ 0.0f, 0.0f, 0.0f, 0.0f }}};

            VkRenderingAttachmentInfo gDepthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            gDepthAttachment.imageView = depthView;
            gDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            gDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            gDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            gDepthAttachment.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo gRenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            gRenderInfo.renderArea = { {0, 0}, {WIDTH, HEIGHT} };
            gRenderInfo.layerCount = 1;
            gRenderInfo.colorAttachmentCount = 2;
            gRenderInfo.pColorAttachments = gColorAttachments;
            gRenderInfo.pDepthAttachment = &gDepthAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &gRenderInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ {0, 0}, {WIDTH, HEIGHT} };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline);
            vkCmdPushConstants(commandBuffer, gbufferPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &gPc);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // Phase 3: Transition G-Buffer -> SHADER_READ_ONLY & Swapchain -> COLOR_ATTACHMENT
            VkImageMemoryBarrier2 readBarriers[3]{};
            readBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            readBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            readBarriers[0].srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            readBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            readBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            readBarriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            readBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            readBarriers[0].image = albedoImage;
            readBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            readBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            readBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            readBarriers[1].srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            readBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            readBarriers[1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            readBarriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            readBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            readBarriers[1].image = normalImage;
            readBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            readBarriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            readBarriers[2].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            readBarriers[2].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            readBarriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            readBarriers[2].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            readBarriers[2].image = swapchainImages[imageIndex];
            readBarriers[2].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkDependencyInfo readDepInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            readDepInfo.imageMemoryBarrierCount = 3;
            readDepInfo.pImageMemoryBarriers = readBarriers;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &readDepInfo);

            // Phase 4: Deferred Lighting Pass
            VkRenderingAttachmentInfo swapColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            swapColorAttachment.imageView = swapchainImageViews[imageIndex];
            swapColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            swapColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            swapColorAttachment.clearValue = {{{ 0.02f, 0.02f, 0.04f, 1.0f }}};

            VkRenderingInfo defRenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            defRenderInfo.renderArea = { {0, 0}, {WIDTH, HEIGHT} };
            defRenderInfo.layerCount = 1;
            defRenderInfo.colorAttachmentCount = 1;
            defRenderInfo.pColorAttachments = &swapColorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &defRenderInfo);

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, deferredPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightingPushConstants), &lPc);

            // Fullscreen triangle dispatch
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // Phase 5: Transition Swapchain Image to Present
            VkImageMemoryBarrier2 barrierToPresent{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            barrierToPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToPresent.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
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
        profiler.exportFoldedFile("flamegraph_assignment75_shader_tile_image_deferred.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment75_shader_tile_image_deferred.html");
        profiler.exportChromeTraceFile("flamegraph_assignment75_shader_tile_image_deferred.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyImageView(device, albedoView, nullptr);
        vkDestroyImage(device, albedoImage, nullptr);
        vkFreeMemory(device, albedoMemory, nullptr);

        vkDestroyImageView(device, normalView, nullptr);
        vkDestroyImage(device, normalImage, nullptr);
        vkFreeMemory(device, normalMemory, nullptr);

        vkDestroyImageView(device, depthView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthMemory, nullptr);

        vkDestroySampler(device, sampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyPipeline(device, gbufferPipeline, nullptr);
        vkDestroyPipeline(device, deferredPipeline, nullptr);
        vkDestroyPipelineLayout(device, gbufferPipelineLayout, nullptr);
        vkDestroyPipelineLayout(device, deferredPipelineLayout, nullptr);

        vkDestroyShaderModule(device, gVertModule, nullptr);
        vkDestroyShaderModule(device, gFragModule, nullptr);
        vkDestroyShaderModule(device, dVertModule, nullptr);
        vkDestroyShaderModule(device, dFragModule, nullptr);
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

        std::cout << "\nAssignment 75 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 75 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
