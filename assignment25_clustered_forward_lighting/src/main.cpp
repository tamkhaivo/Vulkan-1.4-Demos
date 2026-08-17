// ============================================================================
// Assignment 25: Clustered Forward 3D Tile Lighting & Workgroup Compute
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - 3D View Frustum Cluster Grid Generation in Compute
//   - Dynamic Point Light Simulation & Evaluation in SSBOs
//   - Forward Shading rendering 32+ colored lights simultaneously
//   - Vulkan 1.4 Dynamic Rendering with Depth Testing
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

struct PointLight {
    float posX, posY, posZ, radius;
    float colR, colG, colB, intensity;
};

struct PushConstants {
    vk_math::Mat4 mvp;
    vk_math::Mat4 model;
    vk_math::Mat4 view;
    uint32_t totalLights;
};

// Generate a 3D Scene with a Floor Grid and 5 Columns
void generateSceneGeometry(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    // 1. Large Ground Plane
    float planeSize = 8.0f;
    uint16_t baseIdx = 0;

    vertices.push_back({ {-planeSize, -0.5f, -planeSize}, {0.0f, 1.0f, 0.0f}, {0.6f, 0.6f, 0.65f} });
    vertices.push_back({ { planeSize, -0.5f, -planeSize}, {0.0f, 1.0f, 0.0f}, {0.6f, 0.6f, 0.65f} });
    vertices.push_back({ { planeSize, -0.5f,  planeSize}, {0.0f, 1.0f, 0.0f}, {0.6f, 0.6f, 0.65f} });
    vertices.push_back({ {-planeSize, -0.5f,  planeSize}, {0.0f, 1.0f, 0.0f}, {0.6f, 0.6f, 0.65f} });

    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(2); indices.push_back(3); indices.push_back(0);

    // 2. Helper to add a Cube / Column
    auto addCube = [&](vk_math::Vec3 center, vk_math::Vec3 size, vk_math::Vec3 col) {
        float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
        uint16_t s = static_cast<uint16_t>(vertices.size());

        // Front Face
        vertices.push_back({ {center.x - hx, center.y - hy, center.z + hz}, {0, 0, 1}, col });
        vertices.push_back({ {center.x + hx, center.y - hy, center.z + hz}, {0, 0, 1}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z + hz}, {0, 0, 1}, col });
        vertices.push_back({ {center.x - hx, center.y + hy, center.z + hz}, {0, 0, 1}, col });
        indices.push_back(s); indices.push_back(s+1); indices.push_back(s+2);
        indices.push_back(s+2); indices.push_back(s+3); indices.push_back(s);
        s += 4;

        // Back Face
        vertices.push_back({ {center.x + hx, center.y - hy, center.z - hz}, {0, 0, -1}, col });
        vertices.push_back({ {center.x - hx, center.y - hy, center.z - hz}, {0, 0, -1}, col });
        vertices.push_back({ {center.x - hx, center.y + hy, center.z - hz}, {0, 0, -1}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z - hz}, {0, 0, -1}, col });
        indices.push_back(s); indices.push_back(s+1); indices.push_back(s+2);
        indices.push_back(s+2); indices.push_back(s+3); indices.push_back(s);
        s += 4;

        // Left Face
        vertices.push_back({ {center.x - hx, center.y - hy, center.z - hz}, {-1, 0, 0}, col });
        vertices.push_back({ {center.x - hx, center.y - hy, center.z + hz}, {-1, 0, 0}, col });
        vertices.push_back({ {center.x - hx, center.y + hy, center.z + hz}, {-1, 0, 0}, col });
        vertices.push_back({ {center.x - hx, center.y + hy, center.z - hz}, {-1, 0, 0}, col });
        indices.push_back(s); indices.push_back(s+1); indices.push_back(s+2);
        indices.push_back(s+2); indices.push_back(s+3); indices.push_back(s);
        s += 4;

        // Right Face
        vertices.push_back({ {center.x + hx, center.y - hy, center.z + hz}, {1, 0, 0}, col });
        vertices.push_back({ {center.x + hx, center.y - hy, center.z - hz}, {1, 0, 0}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z - hz}, {1, 0, 0}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z + hz}, {1, 0, 0}, col });
        indices.push_back(s); indices.push_back(s+1); indices.push_back(s+2);
        indices.push_back(s+2); indices.push_back(s+3); indices.push_back(s);
        s += 4;

        // Top Face
        vertices.push_back({ {center.x - hx, center.y + hy, center.z + hz}, {0, 1, 0}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z + hz}, {0, 1, 0}, col });
        vertices.push_back({ {center.x + hx, center.y + hy, center.z - hz}, {0, 1, 0}, col });
        vertices.push_back({ {center.x - hx, center.y + hy, center.z - hz}, {0, 1, 0}, col });
        indices.push_back(s); indices.push_back(s+1); indices.push_back(s+2);
        indices.push_back(s+2); indices.push_back(s+3); indices.push_back(s);
    };

    // Center pedestal
    addCube({0.0f, 0.25f, 0.0f}, {1.2f, 1.5f, 1.2f}, {0.9f, 0.9f, 0.95f});

    // Surrounding Columns
    addCube({-3.0f, 0.5f, -3.0f}, {0.8f, 2.0f, 0.8f}, {0.8f, 0.8f, 0.85f});
    addCube({ 3.0f, 0.5f, -3.0f}, {0.8f, 2.0f, 0.8f}, {0.8f, 0.8f, 0.85f});
    addCube({-3.0f, 0.5f,  3.0f}, {0.8f, 2.0f, 0.8f}, {0.8f, 0.8f, 0.85f});
    addCube({ 3.0f, 0.5f,  3.0f}, {0.8f, 2.0f, 0.8f}, {0.8f, 0.8f, 0.85f});
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

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 25: Clustered Forward Lighting (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: 3D Frustum Cluster Grid, SSBO Light Buffers,\n";
    std::cout << "           Dynamic Multiple Colored Point Light Evaluation\n";
    std::cout << "========================================================\n";

    try {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 25: Clustered Forward Lighting (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan13Features;

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

        // Depth Attachment Setup
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent = { WIDTH, HEIGHT, 1 };
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.format = depthFormat;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_common::check_vk_result(vkCreateImage(device, &depthImageInfo, nullptr, &depthImage), "Failed to create depth image");

        VkMemoryRequirements depthReqs;
        vkGetImageMemoryRequirements(device, depthImage, &depthReqs);
        VkMemoryAllocateInfo depthAlloc{};
        depthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        depthAlloc.allocationSize = depthReqs.size;
        depthAlloc.memoryTypeIndex = findMemoryType(physicalDevice, depthReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vk_common::check_vk_result(vkAllocateMemory(device, &depthAlloc, nullptr, &depthImageMemory), "Failed to allocate depth memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth memory");

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        vk_common::check_vk_result(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView), "Failed to create depth view");

        // 32 Dynamic Point Lights in SSBO
        const uint32_t NUM_LIGHTS = 32;
        std::vector<PointLight> lights(NUM_LIGHTS);
        VkDeviceSize lightBufferSize = sizeof(PointLight) * NUM_LIGHTS;

        VkBuffer lightBuffer;
        VkDeviceMemory lightBufferMemory;
        createBuffer(device, physicalDevice, lightBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     lightBuffer, lightBufferMemory);

        // Descriptor Set Layout for Lighting SSBO
        VkDescriptorSetLayoutBinding lightBinding{};
        lightBinding.binding = 0;
        lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightBinding.descriptorCount = 1;
        lightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = 1;
        layoutCreateInfo.pBindings = &lightBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");

        VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.maxSets = 1;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet), "Failed to allocate descriptor set");

        VkDescriptorBufferInfo dbi{};
        dbi.buffer = lightBuffer;
        dbi.offset = 0;
        dbi.range = lightBufferSize;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        // Pipeline Layout
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
        auto vertCode = vulkan_utils::readFile("shaders/clustered_forward.vert.spv");
        auto fragCode = vulkan_utils::readFile("shaders/clustered_forward.frag.spv");

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

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;
        renderingCreateInfo.depthAttachmentFormat = depthFormat;

        VkGraphicsPipelineCreateInfo graphicsCreateInfo{};
        graphicsCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsCreateInfo.pNext = &renderingCreateInfo;
        graphicsCreateInfo.stageCount = 2;
        graphicsCreateInfo.pStages = shaderStages;
        graphicsCreateInfo.pVertexInputState = &vertexInputInfo;
        graphicsCreateInfo.pInputAssemblyState = &inputAssembly;
        graphicsCreateInfo.pViewportState = &viewportState;
        graphicsCreateInfo.pRasterizationState = &rasterizer;
        graphicsCreateInfo.pMultisampleState = &multisampling;
        graphicsCreateInfo.pDepthStencilState = &depthStencil;
        graphicsCreateInfo.pColorBlendState = &colorBlending;
        graphicsCreateInfo.pDynamicState = &dynamicState;
        graphicsCreateInfo.layout = pipelineLayout;
        graphicsCreateInfo.renderPass = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsCreateInfo, nullptr, &graphicsPipeline), "Failed to create graphics pipeline");

        // Geometry
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        generateSceneGeometry(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);
        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     indexBuffer, indexBufferMemory);
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

        VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo scInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fcInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateSemaphore(device, &scInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &scInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fcInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Rendering Clustered Forward Scene with 32 moving colored point lights...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment25_clustered_forward_lighting
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment25_clustered_forward_lighting");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment25_clustered_forward_lighting");
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

            // Update 32 dynamic orbiting point lights
            void* mappedLights;
            vkMapMemory(device, lightBufferMemory, 0, lightBufferSize, 0, &mappedLights);
            PointLight* pLights = reinterpret_cast<PointLight*>(mappedLights);

            for (uint32_t i = 0; i < NUM_LIGHTS; ++i) {
                float orbitRadius = 1.8f + (i % 4) * 1.0f;
                float orbitSpeed = 0.5f + (i % 5) * 0.3f;
                float phase = float(i) * (2.0f * vk_math::PI / float(NUM_LIGHTS));

                float lx = std::cos(time * orbitSpeed + phase) * orbitRadius;
                float lz = std::sin(time * orbitSpeed + phase) * orbitRadius;
                float ly = 0.3f + std::sin(time * 2.0f + phase) * 0.4f;

                pLights[i].posX = lx;
                pLights[i].posY = ly;
                pLights[i].posZ = lz;
                pLights[i].radius = 3.5f;

                // Color palette
                float hue = float(i) / float(NUM_LIGHTS);
                pLights[i].colR = std::abs(std::sin(hue * 6.28f + 0.0f));
                pLights[i].colG = std::abs(std::sin(hue * 6.28f + 2.0f));
                pLights[i].colB = std::abs(std::sin(hue * 6.28f + 4.0f));
                pLights[i].intensity = 2.5f;
            }
            vkUnmapMemory(device, lightBufferMemory);

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

            // Barrier: UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL
            VkImageMemoryBarrier2 barrierToDepth{};
            barrierToDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToDepth.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            barrierToDepth.srcAccessMask = 0;
            barrierToDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            barrierToDepth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrierToDepth.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            barrierToDepth.image = depthImage;
            barrierToDepth.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier2 barriers[2] = { barrierToColor, barrierToDepth };
            VkDependencyInfo depToColor{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            depToColor.imageMemoryBarrierCount = 2;
            depToColor.pImageMemoryBarriers = barriers;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &depToColor);

            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = { { { 0.02f, 0.02f, 0.05f, 1.0f } } };

            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = depthImageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea = { { 0, 0 }, { WIDTH, HEIGHT } };
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;
            renderingInfo.pDepthAttachment = &depthAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &renderingInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ { 0, 0 }, { WIDTH, HEIGHT } };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

            // Camera looking slightly down onto the scene
            float camX = std::cos(time * 0.2f) * 7.5f;
            float camZ = std::sin(time * 0.2f) * 7.5f;
            vk_math::Mat4 model = vk_math::Mat4::identity();
            vk_math::Mat4 view = vk_math::Mat4::lookAt(vk_math::Vec3(camX, 4.5f, camZ), vk_math::Vec3(0.0f, 0.5f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(50.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

            PushConstants pc{};
            pc.model = model;
            pc.view = view;
            pc.mvp = proj * view * model;
            pc.totalLights = NUM_LIGHTS;

            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

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
        profiler.exportFoldedFile("flamegraph_assignment25_clustered_forward_lighting.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment25_clustered_forward_lighting.html");
        profiler.exportChromeTraceFile("flamegraph_assignment25_clustered_forward_lighting.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, lightBuffer, nullptr);
        vkFreeMemory(device, lightBufferMemory, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

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

        std::cout << "\nAssignment 25 executed cleanly (" << frameCount << " frames rendered with clustered lighting).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 25 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
