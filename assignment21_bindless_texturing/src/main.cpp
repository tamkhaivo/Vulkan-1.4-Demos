// ============================================================================
// Assignment 21: Bindless Texturing & Non-Uniform Resource Indexing
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
//   - descriptorBindingPartiallyBound & runtimeDescriptorArray features
//   - shaderSampledImageArrayNonUniformIndexing & GL_EXT_nonuniform_qualifier
//   - Massive unbounded texture arrays (sampler2D globalTextures[])
//   - Dynamic per-draw/per-instance texture indexing via Push Constants
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
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 color;
    Vec2 uv;

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
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, uv);
        return attributeDescriptions;
    }
};

struct PushConstants {
    vk_math::Mat4 mvp;
    uint32_t textureIndex;
};

// Quad geometry for rendering texture grid
const std::vector<Vertex> quadVertices = {
    {{-0.4f, -0.4f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{ 0.4f, -0.4f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{ 0.4f,  0.4f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.4f,  0.4f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> quadIndices = {
    0, 1, 2, 2, 3, 0
};

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

struct TextureResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

TextureResource createProceduralTexture(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, uint32_t patternType) {
    const uint32_t TEX_SIZE = 64;
    std::vector<uint32_t> pixels(TEX_SIZE * TEX_SIZE);

    for (uint32_t y = 0; y < TEX_SIZE; ++y) {
        for (uint32_t x = 0; x < TEX_SIZE; ++x) {
            uint8_t r = 0, g = 0, b = 0, a = 255;
            switch (patternType % 8) {
                case 0: { // Checkerboard (Cyan / Dark Blue)
                    bool check = ((x / 8) + (y / 8)) % 2 == 0;
                    r = check ? 0 : 20;
                    g = check ? 220 : 60;
                    b = check ? 255 : 180;
                    break;
                }
                case 1: { // Concentric rings (Orange / Red)
                    float dist = std::sqrt(float((int)x - 32)*((int)x - 32) + float((int)y - 32)*((int)y - 32));
                    bool ring = int(dist / 4.0f) % 2 == 0;
                    r = ring ? 255 : 180;
                    g = ring ? 140 : 20;
                    b = ring ? 20 : 10;
                    break;
                }
                case 2: { // Diagonal stripes (Green / Lime)
                    bool stripe = ((x + y) / 8) % 2 == 0;
                    r = stripe ? 50 : 10;
                    g = stripe ? 255 : 180;
                    b = stripe ? 100 : 40;
                    break;
                }
                case 3: { // Radial gradient (Magenta / Purple)
                    float dist = std::sqrt(float((int)x - 32)*((int)x - 32) + float((int)y - 32)*((int)y - 32)) / 32.0f;
                    dist = std::min(1.0f, dist);
                    r = uint8_t(255 * (1.0f - dist * 0.5f));
                    g = uint8_t(40 * dist);
                    b = uint8_t(240 * (1.0f - dist * 0.3f));
                    break;
                }
                case 4: { // Grid lines (Yellow / Amber)
                    bool line = (x % 8 == 0) || (y % 8 == 0);
                    r = line ? 255 : 200;
                    g = line ? 240 : 160;
                    b = line ? 20 : 0;
                    break;
                }
                case 5: { // Dot matrix (Pink / White)
                    int dx = (x % 16) - 8;
                    int dy = (y % 16) - 8;
                    bool dot = (dx*dx + dy*dy) < 16;
                    r = dot ? 255 : 80;
                    g = dot ? 255 : 20;
                    b = dot ? 255 : 80;
                    break;
                }
                case 6: { // Horizontal bands (Teal / Turquoise)
                    bool band = (y / 8) % 2 == 0;
                    r = band ? 0 : 30;
                    g = band ? 220 : 160;
                    b = band ? 200 : 120;
                    break;
                }
                default: { // Cross hatch (Violet / Gold)
                    bool hatch = ((x % 8 < 2) || (y % 8 < 2));
                    r = hatch ? 240 : 120;
                    g = hatch ? 200 : 40;
                    b = hatch ? 80 : 180;
                    break;
                }
            }
            pixels[y * TEX_SIZE + x] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    VkDeviceSize imageSize = TEX_SIZE * TEX_SIZE * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    TextureResource tex{};
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { TEX_SIZE, TEX_SIZE, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &tex.image), "Failed to create texture image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, tex.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory), "Failed to allocate image memory");
    vk_common::check_vk_result(vkBindImageMemory(device, tex.image, tex.memory, 0), "Failed to bind image memory");

    // Copy buffer to image using single-time command buffer
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandPool = cmdPool;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { TEX_SIZE, TEX_SIZE, 1 };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // Create Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &tex.view), "Failed to create image view");

    return tex;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 21: Bindless Texturing & Non-Uniform Indexing\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: descriptorBindingPartiallyBound, Non-Uniform Indexing,\n";
    std::cout << "           Unbounded Sampler Arrays, Push Constant Texture IDs\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 21: Bindless Texturing (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        // Enable Bindless & Descriptor Indexing Features
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        indexingFeatures.runtimeDescriptorArray = VK_TRUE;
        indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.pNext = &indexingFeatures;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;
        deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create Vulkan 1.4 device");

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // Swapchain Setup
        VkSurfaceFormatKHR surfaceFormat{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
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
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain view");
        }

        // Command Pool
        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool), "Failed to create command pool");

        // Sampler
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        VkSampler textureSampler;
        vk_common::check_vk_result(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler), "Failed to create sampler");

        // Create 8 distinct procedural textures
        const uint32_t NUM_TEXTURES = 8;
        std::vector<TextureResource> textures(NUM_TEXTURES);
        for (uint32_t i = 0; i < NUM_TEXTURES; ++i) {
            textures[i] = createProceduralTexture(device, physicalDevice, commandPool, graphicsQueue, i);
        }

        // Create Bindless Descriptor Set Layout (Unbounded array of 1024 textures)
        const uint32_t MAX_BINDLESS_TEXTURES = 1024;
        VkDescriptorSetLayoutBinding bindlessBinding{};
        bindlessBinding.binding = 0;
        bindlessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindlessBinding.descriptorCount = MAX_BINDLESS_TEXTURES;
        bindlessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount = 1;
        flagsInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &flagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &bindlessBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout), "Failed to create bindless layout");

        // Descriptor Pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = MAX_BINDLESS_TEXTURES;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        uint32_t variableCount = MAX_BINDLESS_TEXTURES;
        VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
        countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        countInfo.descriptorSetCount = 1;
        countInfo.pDescriptorCounts = &variableCount;

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.pNext = &countInfo;
        setAllocInfo.descriptorPool = descriptorPool;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet bindlessDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &setAllocInfo, &bindlessDescriptorSet), "Failed to allocate descriptor set");

        // Populate Bindless Descriptor Set with the 8 textures
        std::vector<VkDescriptorImageInfo> imageInfos(NUM_TEXTURES);
        for (uint32_t i = 0; i < NUM_TEXTURES; ++i) {
            imageInfos[i].sampler = textureSampler;
            imageInfos[i].imageView = textures[i].view;
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = bindlessDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = NUM_TEXTURES;
        descriptorWrite.pImageInfo = imageInfos.data();

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        std::cout << "[Bindless Table] Successfully populated " << NUM_TEXTURES << " textures into descriptor array slots [0.." << NUM_TEXTURES - 1 << "].\n";

        // Pipeline Layout with Push Constants
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

        // Load Shaders
        auto vertCode = vulkan_utils::readFile("shaders/bindless.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/bindless.frag.spv");

        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragModule;
        shaderStages[1].pName = "main";

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.pNext = &renderingCreateInfo;
        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = shaderStages;
        pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pRasterizationState = &rasterizer;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pColorBlendState = &colorBlending;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // Create Vertex & Index Buffers
        VkDeviceSize vertexBufferSize = sizeof(quadVertices[0]) * quadVertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);
        void* vertData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vertData);
        std::memcpy(vertData, quadVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(quadIndices[0]) * quadIndices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     indexBuffer, indexBufferMemory);
        void* idxData;
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &idxData);
        std::memcpy(idxData, quadIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);

        // Command Buffer & Synchronization
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Rendering 8 textured quads in a 4x2 grid with bindless descriptor indexing...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        // Render Loop
        
        // Initialize Flame Graph Profiler for assignment21_bindless_texturing
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment21_bindless_texturing");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment21_bindless_texturing");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                break;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            vkResetCommandBuffer(commandBuffer, 0);

            VkCommandBufferBeginInfo beginCmd{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginCmd);

            // Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
            VkImageMemoryBarrier2 barrierToColor{};
            barrierToColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToColor.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.srcAccessMask = 0;
            barrierToColor.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToColor.image = swapchainImages[imageIndex];
            barrierToColor.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkDependencyInfo depToColor{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToColor.imageMemoryBarrierCount = 1;
            depToColor.pImageMemoryBarriers = &barrierToColor;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depToColor);

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = { { { 0.08f, 0.08f, 0.12f, 1.0f } } };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bindlessDescriptorSet, 0, nullptr);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Draw a 4x2 grid of quads, each quad selecting a different texture index via push constants
            // Notice: ZERO descriptor set re-binding between draws!
            for (uint32_t i = 0; i < 8; ++i) {
                float posX = -0.675f + (i % 4) * 0.45f;
                float posY = -0.35f + (i / 4) * 0.70f;
                float rotAngle = time * 0.5f + i * 0.4f;

                vk_math::Mat4 model = vk_math::Mat4::translate(vk_math::Vec3(posX, posY, 0.0f)) *
                                     vk_math::Mat4::rotate(rotAngle, vk_math::Vec3(0.0f, 0.0f, 1.0f)) *
                                     vk_math::Mat4::scale(vk_math::Vec3(0.45f, 0.45f, 1.0f));

                PushConstants pc{};
                pc.mvp = model;
                pc.textureIndex = i; // Selects texture from unbounded bindless array

                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
            }

            vk14.vkCmdEndRendering(commandBuffer);

            // Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
            VkImageMemoryBarrier2 barrierToPresent{};
            barrierToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToPresent.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrierToPresent.dstAccessMask = 0;
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
        profiler.exportFoldedFile("flamegraph_assignment21_bindless_texturing.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment21_bindless_texturing.html");
        profiler.exportChromeTraceFile("flamegraph_assignment21_bindless_texturing.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        for (auto& tex : textures) {
            vkDestroyImageView(device, tex.view, nullptr);
            vkDestroyImage(device, tex.image, nullptr);
            vkFreeMemory(device, tex.memory, nullptr);
        }

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
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

        std::cout << "\nAssignment 21 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 21 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
