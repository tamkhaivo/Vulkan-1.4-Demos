// ============================================================================
// Assignment 80: The Ultimate Autonomous Vulkan 1.4 Unified Mega-Engine Capstone
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Synthesis of:
//   - Buffer Device Address (BDA) 64-Bit Descriptorless Geometry
//   - Dynamic Rendering with Depth Testing & Color In-Tile Resolve
//   - Synchronization2 Auto Pipeline Hazards & Present Layout Barriers
//   - Multi-Light PBR Dynamic Forward Model Shading
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

struct alignas(16) Vertex {
    vk_math::Vec3 pos;
    float pad0 = 0.0f;
    vk_math::Vec3 normal;
    float pad1 = 0.0f;
    vk_math::Vec3 color;
    float pad2 = 0.0f;
};

struct MegaPushConstants {
    vk_math::Mat4 mvp;
    vk_math::Mat4 model;
    uint64_t vertexBufferAddress;
    float time;
    uint32_t pad0;
};

// Generate 3D Complex Polyhedral Torus Sphere
void generateMegaGeometry(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    const int RINGS = 48;
    const int SIDES = 24;
    const float INNER_R = 0.35f;
    const float OUTER_R = 0.75f;

    for (int i = 0; i < RINGS; ++i) {
        float u = float(i) * 2.0f * 3.14159265f / float(RINGS);
        for (int j = 0; j < SIDES; ++j) {
            float v = float(j) * 2.0f * 3.14159265f / float(SIDES);

            float x = (OUTER_R + INNER_R * std::cos(v)) * std::cos(u);
            float y = (OUTER_R + INNER_R * std::cos(v)) * std::sin(u);
            float z = INNER_R * std::sin(v);

            vk_math::Vec3 p(x, y, z);
            vk_math::Vec3 center(OUTER_R * std::cos(u), OUTER_R * std::sin(u), 0.0f);
            vk_math::Vec3 normal = (p - center).normalize();

            float r = 0.5f + 0.5f * std::cos(u + 0.0f);
            float g = 0.5f + 0.5f * std::cos(u + 2.0f);
            float b = 0.5f + 0.5f * std::cos(u + 4.0f);

            Vertex vtx{};
            vtx.pos = p;
            vtx.normal = normal;
            vtx.color = { r, g, b };
            vertices.push_back(vtx);
        }
    }

    for (int i = 0; i < RINGS; ++i) {
        int nextI = (i + 1) % RINGS;
        for (int j = 0; j < SIDES; ++j) {
            int nextJ = (j + 1) % SIDES;

            uint32_t c0 = i * SIDES + j;
            uint32_t c1 = i * SIDES + nextJ;
            uint32_t n0 = nextI * SIDES + j;
            uint32_t n1 = nextI * SIDES + nextJ;

            indices.push_back(c0);
            indices.push_back(n0);
            indices.push_back(c1);

            indices.push_back(c1);
            indices.push_back(n0);
            indices.push_back(n1);
        }
    }
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

int main() {
    std::cout << "====================================================================\n";
    std::cout << " Assignment 80: Autonomous Vulkan 1.4 Mega-Engine (Vulkan 1.4 Core)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Synthesis: 64-bit BDA Pointers, Synchronization2, Dynamic Rendering,\n";
    std::cout << "            Dynamic Multi-Light Lighting & Zero-Descriptor Architecture\n";
    std::cout << "====================================================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment 80: Vulkan 1.4 Unified Mega-Engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    try {
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Assignment 80 - Vulkan 1.4 Mega-Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "UnifiedMegaEngine";
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

        VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vulkan12Features.bufferDeviceAddress = VK_TRUE;
        vulkan12Features.scalarBlockLayout = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vulkan13Features.pNext = &vulkan12Features;
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures2.pNext = &vulkan13Features;
        deviceFeatures2.features.shaderInt64 = VK_TRUE;

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

        auto pfnGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
        if (!pfnGetBufferDeviceAddressKHR) {
            pfnGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress");
        }

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
            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat.format;
            viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]), "Failed to create swapchain view");
        }

        // Depth Buffer
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkImageCreateInfo depthImageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
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
        VkMemoryAllocateInfo depthAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        depthAlloc.allocationSize = depthReqs.size;
        depthAlloc.memoryTypeIndex = findMemoryType(physicalDevice, depthReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vk_common::check_vk_result(vkAllocateMemory(device, &depthAlloc, nullptr, &depthImageMemory), "Failed to allocate depth memory");
        vk_common::check_vk_result(vkBindImageMemory(device, depthImage, depthImageMemory, 0), "Failed to bind depth memory");

        VkImageViewCreateInfo depthViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        vk_common::check_vk_result(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView), "Failed to create depth view");

        // Geometry & BDA Allocation
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        generateMegaGeometry(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        {
            VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufferInfo.size = vertexBufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, vertexBuffer, &memReq);
            VkMemoryAllocateFlagsInfo flagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
            flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

            VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocInfo.pNext = &flagsInfo;
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory);
            vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

            void* data;
            vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &data);
            std::memcpy(data, vertices.data(), (size_t)vertexBufferSize);
            vkUnmapMemory(device, vertexBufferMemory);
        }

        VkBufferDeviceAddressInfo bdaInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        bdaInfo.buffer = vertexBuffer;
        VkDeviceAddress vertexBufferAddress = pfnGetBufferDeviceAddressKHR(device, &bdaInfo);
        std::cout << "[BDA 64-bit Address] Vertex Buffer Pointer: 0x" << std::hex << vertexBufferAddress << std::dec << "\n";

        // Index Buffer
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        {
            VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufferInfo.size = indexBufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer);

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, indexBuffer, &memReq);
            VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory);
            vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

            void* data;
            vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &data);
            std::memcpy(data, indices.data(), (size_t)indexBufferSize);
            vkUnmapMemory(device, indexBufferMemory);
        }

        // Push Constant Range
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset = 0;
        pcRange.size = sizeof(MegaPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pcRange;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);

        // Shaders
        std::string shaderDir = "assignment80_ultimate_mega_engine_capstone/shaders/";
        if (!std::filesystem::exists(shaderDir + "mega_engine.vert.spv")) shaderDir = "shaders/";

        auto vertCode = vulkan_utils::readFile(shaderDir + "mega_engine.vert.spv");
        auto fragCode = vulkan_utils::readFile(shaderDir + "mega_engine.frag.spv");

        VkShaderModule vertModule = vulkan_utils::createShaderModule(device, vertCode);
        VkShaderModule fragModule = vulkan_utils::createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr }
        };

        // Fully Descriptorless Vertex Input State (Zero attributes bound!)
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

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

        VkPipelineColorBlendAttachmentState blendState{};
        blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &blendState;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo renderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;
        renderingCreateInfo.depthAttachmentFormat = depthFormat;

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.pNext = &renderingCreateInfo;
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

        VkPipeline graphicsPipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

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

        std::cout << "[Render Loop] Executing Vulkan 1.4 Unified Mega-Engine Capstone Loop...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment80_ultimate_mega_engine_capstone
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment80_ultimate_mega_engine_capstone");
        profiler.initGpu(device, physicalDevice);

        while (!glfwWindowShouldClose(window) && frameCount < 400) {
            VK_PROFILE_SCOPE("assignment80_ultimate_mega_engine_capstone");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) break;

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            vk_math::Mat4 model = vk_math::Mat4::rotate(time * 0.9f, { 0.0f, 1.0f, 0.0f }) * vk_math::Mat4::rotate(time * 0.4f, { 1.0f, 0.0f, 0.0f });
            vk_math::Mat4 view = vk_math::Mat4::lookAt({ 0.0f, 0.5f, 2.4f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
            vk_math::Mat4 proj = vk_math::Mat4::perspective(45.0f * 3.14159265f / 180.0f, float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
            proj.m[1][1] *= -1.0f;

            MegaPushConstants pc{};
            pc.mvp = proj * view * model;
            pc.model = model;
            pc.vertexBufferAddress = vertexBufferAddress;
            pc.time = time;

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginCmd{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginCmd);

            VkImageMemoryBarrier2 barrierToColor{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            barrierToColor.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrierToColor.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrierToColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierToColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrierToColor.image = swapchainImages[imageIndex];
            barrierToColor.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkImageMemoryBarrier2 barrierToDepth{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            barrierToDepth.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
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

            VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = {{{ 0.015f, 0.02f, 0.04f, 1.0f }}};

            VkRenderingAttachmentInfo depthAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            depthAttachmentInfo.imageView = depthImageView;
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            renderInfo.renderArea = { {0, 0}, {WIDTH, HEIGHT} };
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &colorAttachment;
            renderInfo.pDepthAttachment = &depthAttachmentInfo;

            vk14.vkCmdBeginRendering(commandBuffer, &renderInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ {0, 0}, {WIDTH, HEIGHT} };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MegaPushConstants), &pc);

            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

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
        profiler.exportFoldedFile("flamegraph_assignment80_ultimate_mega_engine_capstone.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment80_ultimate_mega_engine_capstone.html");
        profiler.exportChromeTraceFile("flamegraph_assignment80_ultimate_mega_engine_capstone.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

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

        std::cout << "\nAssignment 80 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 80 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
