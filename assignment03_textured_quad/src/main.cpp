// ============================================================================
// Assignment 3: Textured Quad with Sampler
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
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Vertex & Uniform Data Structures
// ----------------------------------------------------------------------------
struct Vertex {
    vk_math::Vec3 pos;
    vk_math::Vec3 color;
    float texCoord[2];

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

        // UV TexCoords (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
};

// Quad Vertices: 2.5D Quad in 3D Space with Colors and UVs
const std::vector<Vertex> quadVertices = {
    {{-0.75f, -0.75f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}, // Top-Left
    {{ 0.75f, -0.75f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}}, // Top-Right
    {{ 0.75f,  0.75f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}, // Bottom-Right
    {{-0.75f,  0.75f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}  // Bottom-Left
};

const std::vector<uint16_t> quadIndices = {
    0, 1, 2,
    2, 3, 0
};

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
// Procedural Checkerboard Pattern Generation (64x64 RGBA8)
// ----------------------------------------------------------------------------
std::vector<uint8_t> generateCheckerboardTexture(uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels(width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t index = (y * width + x) * 4;
            // 8x8 block checkerboard pattern with vibrant tech gradient
            bool isWhite = ((x / 8) + (y / 8)) % 2 == 0;
            if (isWhite) {
                pixels[index + 0] = static_cast<uint8_t>(200 + (x * 55 / width)); // R
                pixels[index + 1] = static_cast<uint8_t>(220 + (y * 35 / height)); // G
                pixels[index + 2] = 255;                                            // B
                pixels[index + 3] = 255;                                            // A
            } else {
                pixels[index + 0] = static_cast<uint8_t>(20 + (x * 40 / width));  // R
                pixels[index + 1] = static_cast<uint8_t>(40 + (y * 50 / height));  // G
                pixels[index + 2] = static_cast<uint8_t>(90 + (x * 60 / width));  // B
                pixels[index + 3] = 255;                                            // A
            }
        }
    }
    return pixels;
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 3: Textured Quad (Vulkan 1.4 Core Standard)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: VkImage, VkSampler, Combined Image Samplers, Vulkan 1.4 Synchronization2" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        // STEP 1: Window & Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 3: Textured Quad (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

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

        // STEP 4: Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 5: Create Texture Image, Allocate Device Memory, Upload via Staging Buffer
        const uint32_t texWidth = 64;
        const uint32_t texHeight = 64;
        std::vector<uint8_t> texturePixels = generateCheckerboardTexture(texWidth, texHeight);
        VkDeviceSize textureDataSize = texWidth * texHeight * 4;

        VkBuffer stagingTextureBuffer;
        VkDeviceMemory stagingTextureMemory;
        createBuffer(device, physicalDevice, textureDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingTextureBuffer, stagingTextureMemory);

        void* texData = nullptr;
        vkMapMemory(device, stagingTextureMemory, 0, textureDataSize, 0, &texData);
        std::memcpy(texData, texturePixels.data(), (size_t)textureDataSize);
        vkUnmapMemory(device, stagingTextureMemory);

        VkImage textureImage;
        VkDeviceMemory textureImageMemory;

        VkImageCreateInfo texImageInfo{};
        texImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        texImageInfo.imageType = VK_IMAGE_TYPE_2D;
        texImageInfo.extent.width = texWidth;
        texImageInfo.extent.height = texHeight;
        texImageInfo.extent.depth = 1;
        texImageInfo.mipLevels = 1;
        texImageInfo.arrayLayers = 1;
        texImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        texImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        texImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        texImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        texImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        texImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        vk_common::check_vk_result(vkCreateImage(device, &texImageInfo, nullptr, &textureImage), "Failed to create texture image");

        VkMemoryRequirements texMemReqs;
        vkGetImageMemoryRequirements(device, textureImage, &texMemReqs);

        VkMemoryAllocateInfo texAllocInfo{};
        texAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        texAllocInfo.allocationSize = texMemReqs.size;
        texAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, texMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vk_common::check_vk_result(vkAllocateMemory(device, &texAllocInfo, nullptr, &textureImageMemory), "Failed to allocate texture image memory");
        vk_common::check_vk_result(vkBindImageMemory(device, textureImage, textureImageMemory, 0), "Failed to bind texture image memory");

        // Copy Staging Buffer to Texture Image using Vulkan 1.4 Synchronization2
        {
            VkCommandBufferAllocateInfo copyCmdAllocInfo{};
            copyCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            copyCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            copyCmdAllocInfo.commandPool = commandPool;
            copyCmdAllocInfo.commandBufferCount = 1;

            VkCommandBuffer copyCmd;
            vkAllocateCommandBuffers(device, &copyCmdAllocInfo, &copyCmd);

            VkCommandBufferBeginInfo cmdBegin{};
            cmdBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmdBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(copyCmd, &cmdBegin);

            // 1. Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                copyCmd,
                vk14.vkCmdPipelineBarrier2,
                textureImage,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // 2. Copy Buffer to Image
            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = {0, 0, 0};
            copyRegion.imageExtent = {texWidth, texHeight, 1};

            vkCmdCopyBufferToImage(copyCmd, stagingTextureBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // 3. Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
            vulkan_utils::pipelineBarrier2ImageTransition(
                copyCmd,
                vk14.vkCmdPipelineBarrier2,
                textureImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            vkEndCommandBuffer(copyCmd);

            VkSubmitInfo copySubmit{};
            copySubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            copySubmit.commandBufferCount = 1;
            copySubmit.pCommandBuffers = &copyCmd;

            vkQueueSubmit(graphicsQueue, 1, &copySubmit, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);

            vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);
        }

        vkDestroyBuffer(device, stagingTextureBuffer, nullptr);
        vkFreeMemory(device, stagingTextureMemory, nullptr);

        // STEP 6: Create Texture Image View & Texture Sampler
        VkImageView textureImageView;
        VkImageViewCreateInfo texViewInfo{};
        texViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        texViewInfo.image = textureImage;
        texViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        texViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        texViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        texViewInfo.subresourceRange.baseMipLevel = 0;
        texViewInfo.subresourceRange.levelCount = 1;
        texViewInfo.subresourceRange.baseArrayLayer = 0;
        texViewInfo.subresourceRange.layerCount = 1;

        vk_common::check_vk_result(vkCreateImageView(device, &texViewInfo, nullptr, &textureImageView), "Failed to create texture image view");

        VkSampler textureSampler;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        vk_common::check_vk_result(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler), "Failed to create texture sampler");

        // STEP 7: Vertex Buffer & Index Buffer Setup
        VkDeviceSize vertexBufferSize = sizeof(quadVertices[0]) * quadVertices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexBufferMemory);

        void* vData = nullptr;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, quadVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingVertexBuffer, vertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);

        VkDeviceSize indexBufferSize = sizeof(quadIndices[0]) * quadIndices.size();
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexBufferMemory);

        void* iData = nullptr;
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &iData);
        std::memcpy(iData, quadIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(device, commandPool, graphicsQueue, stagingIndexBuffer, indexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);

        // STEP 8: Uniform Buffer Setup
        VkDeviceSize uniformBufferSize = sizeof(UniformBufferObject);
        VkBuffer uniformBuffer;
        VkDeviceMemory uniformBufferMemory;
        void* uniformBufferMapped = nullptr;

        createBuffer(device, physicalDevice, uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffer, uniformBufferMemory);
        vkMapMemory(device, uniformBufferMemory, 0, uniformBufferSize, 0, &uniformBufferMapped);

        // STEP 9: Descriptor Set Layout & Descriptors (Binding 0: UBO, Binding 1: Sampler)
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        
        // Binding 0: Uniform Buffer (Vertex stage)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        // Binding 1: Combined Image Sampler (Fragment stage)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

        // Write Descriptors (UBO + Combined Image Sampler)
        VkDescriptorBufferInfo bufferInfoDesc{};
        bufferInfoDesc.buffer = uniformBuffer;
        bufferInfoDesc.offset = 0;
        bufferInfoDesc.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfoDesc{};
        imageInfoDesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoDesc.imageView = textureImageView;
        imageInfoDesc.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        // UBO Write
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfoDesc;

        // Texture Sampler Write
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        // STEP 10: Shaders & Graphics Pipeline
        std::string vertPath = "shaders/quad.vert.spv";
        std::string fragPath = "shaders/quad.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment03_textured_quad/shaders/quad.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment03_textured_quad/shaders/quad.frag.spv";

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
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Allow both sides visible
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // Vulkan 1.4 Dynamic Rendering Pipeline Setup
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
        pipelineInfo.pDepthStencilState = nullptr; // 2D textured quad does not require depth test
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

        std::cout << "Vulkan 1.4 Textured Quad initialized successfully. Entering render loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        // STEP 12: Render Loop
        
        // Initialize Flame Graph Profiler for assignment03_textured_quad
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment03_textured_quad");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment03_textured_quad");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                break;
            }

            // Update Quad Transformation (Gentle Rotation and Pulse)
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            UniformBufferObject ubo{};
            ubo.model = vk_math::Mat4::rotate(time * vk_math::radians(45.0f), vk_math::Vec3(0.0f, 0.0f, 1.0f));
            ubo.view = vk_math::Mat4::lookAt(vk_math::Vec3(0.0f, 0.0f, 2.0f), vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            ubo.proj = vk_math::Mat4::perspective(vk_math::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10.0f);

            std::memcpy(uniformBufferMapped, &ubo, sizeof(ubo));

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

            // Dynamic Rendering Color Attachment
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.06f, 0.07f, 0.11f, 1.0f}};

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = {{0, 0}, {WIDTH, HEIGHT}};
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

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

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);

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
        profiler.exportFoldedFile("flamegraph_assignment03_textured_quad.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment03_textured_quad.html");
        profiler.exportChromeTraceFile("flamegraph_assignment03_textured_quad.json");
        profiler.cleanupGpu();


        // STEP 13: Cleanup Resources
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        vkUnmapMemory(device, uniformBufferMemory);
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

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

    std::cout << "Assignment 3 finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
