// ============================================================================
// Assignment 10: Buffer Device Address and Zero-Copy Streaming
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Buffer Device Address (BDA, Vulkan 1.2+ / 1.4 Core feature `bufferDeviceAddress`)
//   - GL_EXT_buffer_reference shader buffer pointers dereferenced on GPU
//   - Host-Visible + Host-Coherent ReBAR / Zero-Copy Memory Streaming
//   - Push Constant-driven 64-bit GPU Device Addresses (`VkDeviceAddress`)
//   - Ring Buffer Dynamic Vertex Streaming with Fence Frame Synchronization
//   - Real-time CPU-GPU procedural wave mesh deformation without staging copies or descriptors
//   - Vulkan 1.4 Core Dynamic Rendering & Synchronization2
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

// ----------------------------------------------------------------------------
// Data Structures aligned for GL_EXT_buffer_reference & Scalar layout
// ----------------------------------------------------------------------------
struct alignas(16) BdaVertex {
    float pos[4];    // xyz = position, w = u
    float color[4];  // rgba
    float normal[4]; // xyz = normal, w = v
};

struct alignas(16) SceneUniforms {
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    vk_math::Mat4 proj;
    float lightPos[4];
    float viewPos[4];
};

struct PushConstants {
    VkDeviceAddress sceneAddress;     // 64-bit uint64_t pointer to UBO
    VkDeviceAddress vertexAddress;    // 64-bit uint64_t pointer to streamed vertex buffer
    uint32_t        useStreamingData; // 1 = fetch from BDA VertexStream, 0 = traditional
    float           time;
};

// ----------------------------------------------------------------------------
// Helper: Find Memory Type
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

// ----------------------------------------------------------------------------
// Helper: Create BDA-enabled GPU Buffer
// ----------------------------------------------------------------------------
void createBdaBuffer(
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
    bufferInfo.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_common::check_vk_result(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create BDA buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocateFlagsInfo{};
    allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &allocateFlagsInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate BDA memory");
    vk_common::check_vk_result(vkBindBufferMemory(device, buffer, bufferMemory, 0), "Failed to bind BDA memory");
}

VkDeviceAddress getBufferDeviceAddress(VkDevice device, PFN_vkGetBufferDeviceAddress pfnGetAddr, VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;
    return pfnGetAddr(device, &addressInfo);
}

// ----------------------------------------------------------------------------
// Procedural Animated Wave Grid Generation
// ----------------------------------------------------------------------------
constexpr int GRID_RES = 64;
constexpr uint32_t TOTAL_GRID_VERTICES = GRID_RES * GRID_RES * 6; // 2 triangles per quad

void generateWaveGrid(BdaVertex* outVertices, float time) {
    const float size = 4.0f;
    const float step = size / static_cast<float>(GRID_RES);
    const float halfSize = size * 0.5f;

    auto computeHeightAndNormal = [time](float x, float z, float& outY, vk_math::Vec3& outN) {
        float d1 = std::sqrt((x - 0.5f) * (x - 0.5f) + (z - 0.5f) * (z - 0.5f));
        float d2 = std::sqrt((x + 1.0f) * (x + 1.0f) + (z + 1.0f) * (z + 1.0f));
        float wave1 = std::sin(d1 * 4.0f - time * 3.0f) * 0.35f;
        float wave2 = std::cos(d2 * 3.5f + time * 2.5f) * 0.25f;
        float wave3 = std::sin(x * 3.0f + time * 1.5f) * std::cos(z * 3.0f + time * 1.5f) * 0.2f;
        outY = wave1 + wave2 + wave3;

        // Finite differences for normal
        const float eps = 0.02f;
        auto getH = [time](float px, float pz) {
            float pd1 = std::sqrt((px - 0.5f) * (px - 0.5f) + (pz - 0.5f) * (pz - 0.5f));
            float pd2 = std::sqrt((px + 1.0f) * (px + 1.0f) + (pz + 1.0f) * (pz + 1.0f));
            return std::sin(pd1 * 4.0f - time * 3.0f) * 0.35f +
                   std::cos(pd2 * 3.5f + time * 2.5f) * 0.25f +
                   std::sin(px * 3.0f + time * 1.5f) * std::cos(pz * 3.0f + time * 1.5f) * 0.2f;
        };

        float hL = getH(x - eps, z);
        float hR = getH(x + eps, z);
        float hD = getH(x, z - eps);
        float hU = getH(x, z + eps);

        vk_math::Vec3 tangentX(2.0f * eps, hR - hL, 0.0f);
        vk_math::Vec3 tangentZ(0.0f, hU - hD, 2.0f * eps);
        outN = tangentZ.cross(tangentX).normalize();
    };

    uint32_t index = 0;
    for (int gz = 0; gz < GRID_RES; ++gz) {
        for (int gx = 0; gx < GRID_RES; ++gx) {
            float x0 = -halfSize + gx * step;
            float x1 = x0 + step;
            float z0 = -halfSize + gz * step;
            float z1 = z0 + step;

            float y00, y10, y01, y11;
            vk_math::Vec3 n00, n10, n01, n11;

            computeHeightAndNormal(x0, z0, y00, n00);
            computeHeightAndNormal(x1, z0, y10, n10);
            computeHeightAndNormal(x0, z1, y01, n01);
            computeHeightAndNormal(x1, z1, y11, n11);

            auto makeVertex = [](float x, float y, float z, const vk_math::Vec3& n, float u, float v) -> BdaVertex {
                BdaVertex vert{};
                vert.pos[0] = x;
                vert.pos[1] = y;
                vert.pos[2] = z;
                vert.pos[3] = u;

                // Vibrant dynamic colors based on height and slope
                float t = (y + 0.6f) * 0.8f;
                t = std::clamp(t, 0.0f, 1.0f);
                vert.color[0] = 0.1f + 0.9f * t;
                vert.color[1] = 0.3f + 0.7f * (1.0f - std::abs(t - 0.5f) * 2.0f);
                vert.color[2] = 0.9f * (1.0f - t) + 0.2f;
                vert.color[3] = 1.0f;

                vert.normal[0] = n.x;
                vert.normal[1] = n.y;
                vert.normal[2] = n.z;
                vert.normal[3] = v;
                return vert;
            };

            // Quad Triangle 1: (x0,z0) -> (x1,z0) -> (x0,z1)
            outVertices[index++] = makeVertex(x0, y00, z0, n00, static_cast<float>(gx)/GRID_RES, static_cast<float>(gz)/GRID_RES);
            outVertices[index++] = makeVertex(x1, y10, z0, n10, static_cast<float>(gx+1)/GRID_RES, static_cast<float>(gz)/GRID_RES);
            outVertices[index++] = makeVertex(x0, y01, z1, n01, static_cast<float>(gx)/GRID_RES, static_cast<float>(gz+1)/GRID_RES);

            // Quad Triangle 2: (x1,z0) -> (x1,z1) -> (x0,z1)
            outVertices[index++] = makeVertex(x1, y10, z0, n10, static_cast<float>(gx+1)/GRID_RES, static_cast<float>(gz)/GRID_RES);
            outVertices[index++] = makeVertex(x1, y11, z1, n11, static_cast<float>(gx+1)/GRID_RES, static_cast<float>(gz+1)/GRID_RES);
            outVertices[index++] = makeVertex(x0, y01, z1, n01, static_cast<float>(gx)/GRID_RES, static_cast<float>(gz+1)/GRID_RES);
        }
    }
}

// ----------------------------------------------------------------------------
// Main Application
// ----------------------------------------------------------------------------
int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 10: Buffer Device Address (BDA) & Streaming" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: Buffer Device Address (BDA), GL_EXT_buffer_reference," << std::endl;
    std::cout << "          Host-Visible Zero-Copy Streaming & Ring Buffers" << std::endl;
    std::cout << "========================================================" << std::endl;

    constexpr int WIDTH = 1280;
    constexpr int HEIGHT = 720;
    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    try {
        // STEP 1: Window, Instance, Surface, Physical Device, Device
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 10: Buffer Device Address & Streaming (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (!vk14.vkGetBufferDeviceAddress) {
            throw std::runtime_error("vkGetBufferDeviceAddress is not available on this Vulkan 1.4 driver!");
        }

        std::cout << "Vulkan 1.4 Logical Device initialized with BDA and Dynamic Rendering." << std::endl;

        // STEP 2: Swapchain
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

        // STEP 3: Depth Buffer
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

        // STEP 4: Zero-Copy Host-Visible BDA Streaming Buffers (Ring-buffered per frame)
        VkDeviceSize vertexBufferSize = sizeof(BdaVertex) * TOTAL_GRID_VERTICES;
        VkDeviceSize uboBufferSize = sizeof(SceneUniforms);

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> streamedVertexBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> streamedVertexMemories;
        std::array<BdaVertex*, MAX_FRAMES_IN_FLIGHT> mappedVertexPtrs;
        std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> vertexDeviceAddresses;

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uboBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uboMemories;
        std::array<SceneUniforms*, MAX_FRAMES_IN_FLIGHT> mappedUboPtrs;
        std::array<VkDeviceAddress, MAX_FRAMES_IN_FLIGHT> uboDeviceAddresses;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            // Vertex streaming buffer (Host-Visible + Host-Coherent + Shader Device Address)
            createBdaBuffer(
                device, physicalDevice,
                vertexBufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                streamedVertexBuffers[i], streamedVertexMemories[i]
            );
            vkMapMemory(device, streamedVertexMemories[i], 0, vertexBufferSize, 0, reinterpret_cast<void**>(&mappedVertexPtrs[i]));
            vertexDeviceAddresses[i] = getBufferDeviceAddress(device, vk14.vkGetBufferDeviceAddress, streamedVertexBuffers[i]);

            // UBO buffer with BDA
            createBdaBuffer(
                device, physicalDevice,
                uboBufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uboBuffers[i], uboMemories[i]
            );
            vkMapMemory(device, uboMemories[i], 0, uboBufferSize, 0, reinterpret_cast<void**>(&mappedUboPtrs[i]));
            uboDeviceAddresses[i] = getBufferDeviceAddress(device, vk14.vkGetBufferDeviceAddress, uboBuffers[i]);

            std::cout << "[Frame " << i << "] Streamed Vertex Buffer GPU Address: 0x" << std::hex << vertexDeviceAddresses[i] << std::dec << std::endl;
            std::cout << "[Frame " << i << "] Scene UBO GPU Address:            0x" << std::hex << uboDeviceAddresses[i] << std::dec << std::endl;
        }

        // STEP 5: Pipeline Layout with Push Constants (NO DESCRIPTOR SETS NEEDED!)
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Zero descriptor set layouts!
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 6: Load Shaders and Create Graphics Pipeline
        std::string vertPath = "assignment10_buffer_device_address_streaming/shaders/bda_stream.vert.spv";
        std::string fragPath = "assignment10_buffer_device_address_streaming/shaders/bda_stream.frag.spv";
        if (!std::filesystem::exists(vertPath)) vertPath = "shaders/bda_stream.vert.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "shaders/bda_stream.frag.spv";

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

        // Note: When reading vertices via BDA buffer reference in shader, vertex input state can be completely empty!
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
        rasterizer.cullMode = VK_CULL_MODE_NONE; // Double-sided terrain wave
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

        // Vulkan 1.4 Dynamic Rendering Pipeline Setup
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

        // STEP 7: Command Pool, Buffers & Sync Objects
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

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
            vk_common::check_vk_result(vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphores[i]), "Failed to create sem");
            vk_common::check_vk_result(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphores[i]), "Failed to create sem");
            vk_common::check_vk_result(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]), "Failed to create fence");
        }

        std::cout << "Buffer Device Address Streaming Pipeline ready. Starting dynamic streaming render loop..." << std::endl;

        // STEP 8: Main Render Loop with Zero-Copy CPU->GPU Vertex Streaming
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
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

            // Zero-Copy Streaming: Directly write new deformed mesh data into host-visible mapped memory!
            generateWaveGrid(mappedVertexPtrs[currentFrame], time);

            // Update Scene UBO directly via mapped memory
            SceneUniforms& ubo = *mappedUboPtrs[currentFrame];
            ubo.model = vk_math::Mat4::rotate(time * 0.25f, vk_math::Vec3(0.0f, 1.0f, 0.0f));
            
            vk_math::Vec3 eyePos(3.5f * std::cos(time * 0.15f), 3.0f, 3.5f * std::sin(time * 0.15f));
            ubo.view = vk_math::Mat4::lookAt(eyePos, vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            ubo.proj = vk_math::Mat4::perspective(vk_math::radians(50.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
            
            ubo.lightPos[0] = 3.0f * std::cos(time * 0.8f);
            ubo.lightPos[1] = 4.0f;
            ubo.lightPos[2] = 3.0f * std::sin(time * 0.8f);
            ubo.lightPos[3] = 1.0f;

            ubo.viewPos[0] = eyePos.x;
            ubo.viewPos[1] = eyePos.y;
            ubo.viewPos[2] = eyePos.z;
            ubo.viewPos[3] = 1.0f;

            // Command Buffer Recording
            VkCommandBuffer cmd = commandBuffers[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &beginInfo);

            // Synchronization2 Layout Transitions for Swapchain Image & Depth Image
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
            colorAttachment.clearValue.color = {{0.03f, 0.05f, 0.09f, 1.0f}}; // Dark modern slate

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

            // Pass 64-bit raw GPU device addresses via Push Constants
            PushConstants pc{};
            pc.sceneAddress = uboDeviceAddresses[currentFrame];
            pc.vertexAddress = vertexDeviceAddresses[currentFrame];
            pc.useStreamingData = 1; // Direct BDA vertex stream reading
            pc.time = time;

            vkCmdPushConstants(
                cmd,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstants),
                &pc
            );

            // Draw direct vertex stream without binding vertex or index buffers!
            vkCmdDraw(cmd, TOTAL_GRID_VERTICES, 1, 0, 0);

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

            VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
                // Swapchain resized or out of date upon window close
                break;
            }

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        vkDeviceWaitIdle(device);

        // STEP 9: Cleanup Resources
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkUnmapMemory(device, streamedVertexMemories[i]);
            vkDestroyBuffer(device, streamedVertexBuffers[i], nullptr);
            vkFreeMemory(device, streamedVertexMemories[i], nullptr);

            vkUnmapMemory(device, uboMemories[i]);
            vkDestroyBuffer(device, uboBuffers[i], nullptr);
            vkFreeMemory(device, uboMemories[i], nullptr);

            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

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

    std::cout << "Assignment 10 executed cleanly." << std::endl;
    return EXIT_SUCCESS;
}
