// ============================================================================
// Assignment 7: Compute Particle System with Indirect Draw
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Compute Pipeline with Compute Shaders (`particle.comp`)
//   - Shader Storage Buffer Objects (SSBO) for high-performance GPU particle states
//   - GPU Indirect Drawing (`vkCmdDrawIndirect` with `VkDrawIndirectCommand`)
//   - Vulkan 1.4 Synchronization2 (Compute Write -> Graphics Vertex / Indirect Read barriers)
//   - Dynamic Rendering with Additive Blending for volumetric particle luminescence
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
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Particle & Uniform Structures
// ----------------------------------------------------------------------------
struct Particle {
    float pos[4]; // xyz = pos, w = size
    float vel[4]; // xyz = vel, w = life
    float col[4]; // rgba
};

struct SceneUBO {
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
    float cameraPos[4];
};

struct ComputePushConstants {
    float dt;
    float time;
    uint32_t particleCount;
    uint32_t resetTrigger;
};

// ----------------------------------------------------------------------------
// Buffer & Memory Helper Functions
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
// Initial Particle Generation
// ----------------------------------------------------------------------------
static std::vector<Particle> generateInitialParticles(uint32_t count) {
    std::vector<Particle> particles(count);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f);

    for (uint32_t i = 0; i < count; ++i) {
        float angle = distAngle(rng);
        float radius = 0.3f + dist01(rng) * 2.5f;
        float y = (dist01(rng) - 0.5f) * 1.5f;

        particles[i].pos[0] = std::cos(angle) * radius;
        particles[i].pos[1] = y;
        particles[i].pos[2] = std::sin(angle) * radius;
        particles[i].pos[3] = 0.04f + dist01(rng) * 0.04f; // base billboard size

        // Tangential velocity around vortex
        float speed = 1.0f + dist01(rng) * 1.5f;
        particles[i].vel[0] = -std::sin(angle) * speed;
        particles[i].vel[1] = (dist01(rng) - 0.3f) * 0.5f;
        particles[i].vel[2] = std::cos(angle) * speed;
        particles[i].vel[3] = dist01(rng); // initial life progress [0, 1]

        particles[i].col[0] = 0.2f + dist01(rng) * 0.8f;
        particles[i].col[1] = 0.4f + dist01(rng) * 0.6f;
        particles[i].col[2] = 1.0f;
        particles[i].col[3] = 1.0f;
    }

    return particles;
}

// ----------------------------------------------------------------------------
// Main Application Entry Point
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 7: Compute Particles & Indirect Draw (Vulkan 1.4)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: Compute Pipelines, SSBOs, vkCmdDrawIndirect, Vulkan 1.4 Synchronization2" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;
        const uint32_t PARTICLE_COUNT = 65536; // 64K particles simulated real-time on GPU!

        std::cout << "Simulating " << PARTICLE_COUNT << " particles dynamically with Compute Shader & Indirect Draw." << std::endl;

        // STEP 1: Window & Vulkan 1.4 Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 7: Compute Particles & Indirect Draw (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // STEP 2: Logical Device with Dynamic Rendering, Synchronization2, multiDrawIndirect
        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                presentSupport) {
                graphicsQueueFamily = i;
                break;
            }
        }

        if (graphicsQueueFamily == UINT32_MAX) {
            throw std::runtime_error("Could not find a queue family supporting graphics, compute, and presentation!");
        }

        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        VkQueue queue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &queue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (!vk14.vkCmdBeginRendering || !vk14.vkCmdEndRendering || !vk14.vkCmdPipelineBarrier2) {
            throw std::runtime_error("Failed to load Vulkan 1.4 core dynamic rendering & synchronization2 pointers!");
        }

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

        // STEP 4: Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        // STEP 5: Particle SSBO Buffer & Indirect Draw Command Buffer
        std::vector<Particle> initialParticles = generateInitialParticles(PARTICLE_COUNT);
        VkDeviceSize particleBufferSize = sizeof(Particle) * PARTICLE_COUNT;

        VkBuffer stagingParticleBuffer;
        VkDeviceMemory stagingParticleMemory;
        createBuffer(device, physicalDevice, particleBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingParticleBuffer, stagingParticleMemory);

        void* pData = nullptr;
        vkMapMemory(device, stagingParticleMemory, 0, particleBufferSize, 0, &pData);
        std::memcpy(pData, initialParticles.data(), (size_t)particleBufferSize);
        vkUnmapMemory(device, stagingParticleMemory);

        // Particle Buffer (Storage Buffer for Compute + Storage Buffer for Graphics Vertex)
        VkBuffer particleBuffer;
        VkDeviceMemory particleBufferMemory;
        createBuffer(device, physicalDevice, particleBufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     particleBuffer, particleBufferMemory);

        copyBuffer(device, commandPool, queue, stagingParticleBuffer, particleBuffer, particleBufferSize);
        vkDestroyBuffer(device, stagingParticleBuffer, nullptr);
        vkFreeMemory(device, stagingParticleMemory, nullptr);

        // Indirect Draw Command Buffer: Holds VkDrawIndirectCommand
        VkDeviceSize indirectBufferSize = sizeof(VkDrawIndirectCommand);
        VkBuffer indirectBuffer;
        VkDeviceMemory indirectBufferMemory;
        createBuffer(device, physicalDevice, indirectBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     indirectBuffer, indirectBufferMemory);

        // STEP 6: Uniform Buffer for Graphics Scene View/Projection
        VkDeviceSize sceneUBOSize = sizeof(SceneUBO);
        VkBuffer sceneUBOBuffer;
        VkDeviceMemory sceneUBOBufferMemory;
        void* sceneUBOMapped = nullptr;

        createBuffer(device, physicalDevice, sceneUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sceneUBOBuffer, sceneUBOBufferMemory);
        vkMapMemory(device, sceneUBOBufferMemory, 0, sceneUBOSize, 0, &sceneUBOMapped);

        // STEP 7: Descriptor Set Layouts & Pools
        // Compute Descriptor Set Layout: Binding 0 = Particle SSBO, Binding 1 = Indirect Buffer SSBO
        std::array<VkDescriptorSetLayoutBinding, 2> computeBindings{};
        computeBindings[0].binding = 0;
        computeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeBindings[0].descriptorCount = 1;
        computeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        computeBindings[1].binding = 1;
        computeBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeBindings[1].descriptorCount = 1;
        computeBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
        computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        computeLayoutInfo.bindingCount = static_cast<uint32_t>(computeBindings.size());
        computeLayoutInfo.pBindings = computeBindings.data();

        VkDescriptorSetLayout computeDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout), "Failed to create compute descriptor set layout");

        // Graphics Descriptor Set Layout: Binding 0 = Scene UBO, Binding 1 = Particle SSBO (Readonly)
        std::array<VkDescriptorSetLayoutBinding, 2> graphicsBindings{};
        graphicsBindings[0].binding = 0;
        graphicsBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        graphicsBindings[0].descriptorCount = 1;
        graphicsBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        graphicsBindings[1].binding = 1;
        graphicsBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        graphicsBindings[1].descriptorCount = 1;
        graphicsBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo{};
        graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        graphicsLayoutInfo.bindingCount = static_cast<uint32_t>(graphicsBindings.size());
        graphicsLayoutInfo.pBindings = graphicsBindings.data();

        VkDescriptorSetLayout graphicsDescriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &graphicsLayoutInfo, nullptr, &graphicsDescriptorSetLayout), "Failed to create graphics descriptor set layout");

        // Descriptor Pool
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = 4;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 2;

        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descPoolInfo.pPoolSizes = poolSizes.data();
        descPoolInfo.maxSets = 4;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        // Allocate Compute Descriptor Set
        VkDescriptorSetAllocateInfo computeAllocInfo{};
        computeAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        computeAllocInfo.descriptorPool = descriptorPool;
        computeAllocInfo.descriptorSetCount = 1;
        computeAllocInfo.pSetLayouts = &computeDescriptorSetLayout;

        VkDescriptorSet computeDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &computeAllocInfo, &computeDescriptorSet), "Failed to allocate compute descriptor set");

        VkDescriptorBufferInfo particleBufferInfoDesc{};
        particleBufferInfoDesc.buffer = particleBuffer;
        particleBufferInfoDesc.offset = 0;
        particleBufferInfoDesc.range = particleBufferSize;

        VkDescriptorBufferInfo indirectBufferInfoDesc{};
        indirectBufferInfoDesc.buffer = indirectBuffer;
        indirectBufferInfoDesc.offset = 0;
        indirectBufferInfoDesc.range = indirectBufferSize;

        std::array<VkWriteDescriptorSet, 2> computeWrites{};
        computeWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeWrites[0].dstSet = computeDescriptorSet;
        computeWrites[0].dstBinding = 0;
        computeWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeWrites[0].descriptorCount = 1;
        computeWrites[0].pBufferInfo = &particleBufferInfoDesc;

        computeWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeWrites[1].dstSet = computeDescriptorSet;
        computeWrites[1].dstBinding = 1;
        computeWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        computeWrites[1].descriptorCount = 1;
        computeWrites[1].pBufferInfo = &indirectBufferInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWrites.size()), computeWrites.data(), 0, nullptr);

        // Allocate Graphics Descriptor Set
        VkDescriptorSetAllocateInfo graphicsAllocInfo{};
        graphicsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        graphicsAllocInfo.descriptorPool = descriptorPool;
        graphicsAllocInfo.descriptorSetCount = 1;
        graphicsAllocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        VkDescriptorSet graphicsDescriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &graphicsAllocInfo, &graphicsDescriptorSet), "Failed to allocate graphics descriptor set");

        VkDescriptorBufferInfo sceneUBOBufferInfoDesc{};
        sceneUBOBufferInfoDesc.buffer = sceneUBOBuffer;
        sceneUBOBufferInfoDesc.offset = 0;
        sceneUBOBufferInfoDesc.range = sceneUBOSize;

        std::array<VkWriteDescriptorSet, 2> graphicsWrites{};
        graphicsWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        graphicsWrites[0].dstSet = graphicsDescriptorSet;
        graphicsWrites[0].dstBinding = 0;
        graphicsWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        graphicsWrites[0].descriptorCount = 1;
        graphicsWrites[0].pBufferInfo = &sceneUBOBufferInfoDesc;

        graphicsWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        graphicsWrites[1].dstSet = graphicsDescriptorSet;
        graphicsWrites[1].dstBinding = 1;
        graphicsWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        graphicsWrites[1].descriptorCount = 1;
        graphicsWrites[1].pBufferInfo = &particleBufferInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(graphicsWrites.size()), graphicsWrites.data(), 0, nullptr);

        // STEP 8: Pipeline Layouts & Pipelines
        // Compute Pipeline Layout with Push Constants
        VkPushConstantRange computePushRange{};
        computePushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        computePushRange.offset = 0;
        computePushRange.size = sizeof(ComputePushConstants);

        VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
        computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computePipelineLayoutInfo.setLayoutCount = 1;
        computePipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        computePipelineLayoutInfo.pushConstantRangeCount = 1;
        computePipelineLayoutInfo.pPushConstantRanges = &computePushRange;

        VkPipelineLayout computePipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &computePipelineLayoutInfo, nullptr, &computePipelineLayout), "Failed to create compute pipeline layout");

        // Load Shaders
        std::string compPath = "shaders/particle.comp.spv";
        std::string vertPath = "shaders/particle.vert.spv";
        std::string fragPath = "shaders/particle.frag.spv";
        if (!std::filesystem::exists(compPath)) compPath = "assignment07_compute_particles_indirect_draw/shaders/particle.comp.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "assignment07_compute_particles_indirect_draw/shaders/particle.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "assignment07_compute_particles_indirect_draw/shaders/particle.frag.spv";

        auto compCode = vulkan_utils::readFile(compPath);
        auto vertCode = vulkan_utils::readFile(vertPath);
        auto fragCode = vulkan_utils::readFile(fragPath);

        VkShaderModule compModule = vulkan_utils::createShaderModule(device, compCode);
        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        // Create Compute Pipeline
        VkComputePipelineCreateInfo computePipelineInfo{};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compModule, "main", nullptr};
        computePipelineInfo.layout = computePipelineLayout;

        VkPipeline computePipeline;
        vk_common::check_vk_result(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &computePipeline), "Failed to create compute pipeline");

        // Graphics Pipeline Layout
        VkPipelineLayoutCreateInfo graphicsPipelineLayoutInfo{};
        graphicsPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        graphicsPipelineLayoutInfo.setLayoutCount = 1;
        graphicsPipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        VkPipelineLayout graphicsPipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &graphicsPipelineLayoutInfo, nullptr, &graphicsPipelineLayout), "Failed to create graphics pipeline layout");

        // Graphics Pipeline Setup: Billboards drawn via gl_VertexIndex (Triangle Strip)
        VkPipelineShaderStageCreateInfo graphicsShaderStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Billboards double-sided

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Particle Blending: Additive Alpha Blending for glowing energy particles
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE; // Additive glow
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Vulkan 1.4 Dynamic Rendering Pipeline Setup
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
        pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

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
        graphicsPipelineInfo.pColorBlendState = &colorBlending;
        graphicsPipelineInfo.pDynamicState = &dynamicState;
        graphicsPipelineInfo.layout = graphicsPipelineLayout;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // STEP 9: Command Buffer & Synchronization Primitives
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

        std::cout << "Assignment 7: Compute Particle & Indirect Draw pipeline initialized. Starting render loop..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();
        auto lastFrameTime = startTime;

        // STEP 10: Simulation & Render Loop
        
        // Initialize Flame Graph Profiler for assignment07_compute_particles_indirect_draw
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment07_compute_particles_indirect_draw");
        profiler.initGpu(device, physicalDevice);


        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment07_compute_particles_indirect_draw");
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
            float dt = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;

            if (dt > 0.1f) dt = 0.016f; // Clamp delta time during startup

            // Update Scene Camera UBO
            float camRadius = 4.8f;
            float camAngle = time * 0.25f;
            vk_math::Vec3 eyePos(std::cos(camAngle) * camRadius, 1.8f + std::sin(time * 0.4f) * 0.8f, std::sin(camAngle) * camRadius);

            SceneUBO ubo{};
            ubo.view = vk_math::Mat4::lookAt(eyePos, vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            ubo.proj = vk_math::Mat4::perspective(
                vk_math::radians(55.0f),
                static_cast<float>(WIDTH) / static_cast<float>(HEIGHT),
                0.1f,
                100.0f
            );
            ubo.cameraPos[0] = eyePos.x;
            ubo.cameraPos[1] = eyePos.y;
            ubo.cameraPos[2] = eyePos.z;
            ubo.cameraPos[3] = 1.0f;
            std::memcpy(sceneUBOMapped, &ubo, sizeof(ubo));

            // Record Command Buffer
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // ================================================================
            // COMPUTE PASS: Simulate Particles & Generate Indirect Draw Params
            // ================================================================
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

            ComputePushConstants compPush{};
            compPush.dt = dt;
            compPush.time = time;
            compPush.particleCount = PARTICLE_COUNT;
            compPush.resetTrigger = 0;
            vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &compPush);

            uint32_t groupCountX = (PARTICLE_COUNT + 255) / 256;
            vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

            // ================================================================
            // SYNCHRONIZATION: PipelineBarrier2 (Compute Write -> Graphics Read)
            // Synchronize Particle SSBO & Indirect Draw Command Buffer
            // ================================================================
            std::array<VkBufferMemoryBarrier2, 2> bufferBarriers{};
            // Particle SSBO Barrier: Compute Shader Write -> Vertex Shader Read
            bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            bufferBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            bufferBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            bufferBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            bufferBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[0].buffer = particleBuffer;
            bufferBarriers[0].offset = 0;
            bufferBarriers[0].size = particleBufferSize;

            // Indirect Buffer Barrier: Compute Shader Write -> Indirect Command Read
            bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            bufferBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            bufferBarriers[1].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            bufferBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            bufferBarriers[1].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarriers[1].buffer = indirectBuffer;
            bufferBarriers[1].offset = 0;
            bufferBarriers[1].size = indirectBufferSize;

            VkDependencyInfo computeToGraphicsDependency{};
            computeToGraphicsDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            computeToGraphicsDependency.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
            computeToGraphicsDependency.pBufferMemoryBarriers = bufferBarriers.data();

            vk14.vkCmdPipelineBarrier2(commandBuffer, &computeToGraphicsDependency);

            // ================================================================
            // GRAPHICS PASS: Dynamic Rendering & Indirect Drawing
            // ================================================================
            // Transition Swapchain Image for Dynamic Rendering Color Output
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

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {{0.01f, 0.015f, 0.035f, 1.0f}}; // Deep Space Obsidian Background

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

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsDescriptorSet, 0, nullptr);

            // Execute GPU Indirect Draw Command (Reading draw count & instances directly from GPU indirect buffer)
            vkCmdDrawIndirect(commandBuffer, indirectBuffer, 0, 1, sizeof(VkDrawIndirectCommand));

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

            vkQueueSubmit(queue, 1, &submitInfo, inFlightFence);

            // Present Swapchain Image
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(queue, &presentInfo);
        }

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment07_compute_particles_indirect_draw.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment07_compute_particles_indirect_draw.html");
        profiler.exportChromeTraceFile("flamegraph_assignment07_compute_particles_indirect_draw.json");
        profiler.cleanupGpu();


        // STEP 11: Cleanup Resources
        vkUnmapMemory(device, sceneUBOBufferMemory);
        vkDestroyBuffer(device, sceneUBOBuffer, nullptr);
        vkFreeMemory(device, sceneUBOBufferMemory, nullptr);

        vkDestroyBuffer(device, indirectBuffer, nullptr);
        vkFreeMemory(device, indirectBufferMemory, nullptr);

        vkDestroyBuffer(device, particleBuffer, nullptr);
        vkFreeMemory(device, particleBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);

        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);

        vkDestroyShaderModule(device, compModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);

        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

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

    std::cout << "Assignment 7 finished cleanly." << std::endl;
    return EXIT_SUCCESS;
}
