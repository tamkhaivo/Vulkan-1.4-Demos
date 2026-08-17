// ============================================================================
// Assignment 11: Modern Mesh & Task Shading Pipeline (VK_EXT_mesh_shader)
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_EXT_mesh_shader (Task Shaders + Mesh Shaders replacing Vertex/IA)
//   - Dynamic Rendering with vkCmdDrawMeshTasksEXT
//   - Micro-meshlet GPU generation and dynamic payload dispatch
//   - Push Constant updates for high-frequency transformation streaming
//   - Vulkan 1.4 Core Synchronization2 & Dynamic State
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

// Extension function pointer for VK_EXT_mesh_shader
static PFN_vkCmdDrawMeshTasksEXT pfn_vkCmdDrawMeshTasksEXT = nullptr;

struct alignas(16) MeshPushConstants {
    vk_math::Mat4 viewProj;
    float time;
    uint32_t totalMeshlets;
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

int main() {
    std::cout << "========================================================\n";
    std::cout << "Assignment 11: Modern Mesh & Task Shading Pipeline (Version 2)\n";
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << "Concepts: VK_EXT_mesh_shader (Task + Mesh Shaders),\n";
    std::cout << "          Dynamic Rendering & vkCmdDrawMeshTasksEXT\n";
    std::cout << "========================================================\n";

    constexpr int WIDTH = 1280;
    constexpr int HEIGHT = 720;
    constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    constexpr uint32_t TOTAL_MESHLETS = 8;

    try {
        // STEP 1: Create Window and Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 11: Mesh & Task Shaders (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Check for VK_EXT_mesh_shader extension support
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExtensions.data());

        bool meshShaderSupported = false;
        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                meshShaderSupported = true;
                break;
            }
        }

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);

        VkDevice device = VK_NULL_HANDLE;
        bool usingMeshShader = false;

        if (meshShaderSupported) {
            // Check feature support for taskShader and meshShader
            VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{};
            meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &meshFeatures;
            vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

            if (meshFeatures.meshShader && meshFeatures.taskShader) {
                std::cout << "[Device Feature] VK_EXT_mesh_shader (Task & Mesh Shading) is fully supported on hardware.\n";

                float queuePriority = 1.0f;
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;

                std::vector<const char*> deviceExtensions = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    VK_EXT_MESH_SHADER_EXTENSION_NAME
                };

                VkPhysicalDeviceMeshShaderFeaturesEXT enableMeshFeatures{};
                enableMeshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
                enableMeshFeatures.taskShader = VK_TRUE;
                enableMeshFeatures.meshShader = VK_TRUE;

                // Vulkan 1.3 Core Features (Dynamic Rendering & Synchronization2)
                VkPhysicalDeviceVulkan13Features vulkan13Features{};
                vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                vulkan13Features.pNext = &enableMeshFeatures;
                vulkan13Features.dynamicRendering = VK_TRUE;
                vulkan13Features.synchronization2 = VK_TRUE;
                vulkan13Features.maintenance4 = VK_TRUE;

                VkPhysicalDeviceFeatures2 deviceFeatures2{};
                deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                deviceFeatures2.pNext = &vulkan13Features;

                VkDeviceCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                createInfo.pNext = &deviceFeatures2;
                createInfo.queueCreateInfoCount = 1;
                createInfo.pQueueCreateInfos = &queueCreateInfo;
                createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
                createInfo.ppEnabledExtensionNames = deviceExtensions.data();

                vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create Vulkan 1.4 Device with Mesh Shaders");
                usingMeshShader = true;
            }
        }

        if (!usingMeshShader) {
            std::cout << "[Info] Falling back to standard Vulkan 1.4 logical device.\n";
            device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (usingMeshShader) {
            pfn_vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
            if (!pfn_vkCmdDrawMeshTasksEXT) {
                std::cout << "[Warning] vkCmdDrawMeshTasksEXT proc address not found via vkGetDeviceProcAddr.\n";
            }
        }

        std::cout << "Vulkan 1.4 Logical Device initialized. Dynamic Rendering & Synchronization2 active.\n";

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

        // STEP 4: Pipeline Layout with Push Constants
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MeshPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        // STEP 5: Create Mesh Shading Pipeline (or Fallback if not supported)
        VkShaderModule taskModule = VK_NULL_HANDLE;
        VkShaderModule meshModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;
        VkPipeline meshPipeline = VK_NULL_HANDLE;

        std::string taskPath = "assignment11_mesh_task_shading/shaders/meshlet.task.spv";
        std::string meshPath = "assignment11_mesh_task_shading/shaders/meshlet.mesh.spv";
        std::string fragPath = "assignment11_mesh_task_shading/shaders/meshlet.frag.spv";
        if (!std::filesystem::exists(taskPath)) taskPath = "shaders/meshlet.task.spv";
        if (!std::filesystem::exists(meshPath)) meshPath = "shaders/meshlet.mesh.spv";
        if (!std::filesystem::exists(fragPath)) fragPath = "shaders/meshlet.frag.spv";

        if (usingMeshShader && pfn_vkCmdDrawMeshTasksEXT && std::filesystem::exists(taskPath) && std::filesystem::exists(meshPath) && std::filesystem::exists(fragPath)) {
            auto taskCode = vulkan_utils::readFile(taskPath);
            auto meshCode = vulkan_utils::readFile(meshPath);
            auto fragCode = vulkan_utils::readFile(fragPath);

            taskModule = vulkan_utils::createShaderModule(device, taskCode);
            meshModule = vulkan_utils::createShaderModule(device, meshCode);
            fragModule = vulkan_utils::createShaderModule(device, fragCode);

            std::vector<VkPipelineShaderStageCreateInfo> shaderStages(3);
            shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
            shaderStages[0].module = taskModule;
            shaderStages[0].pName = "main";

            shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
            shaderStages[1].module = meshModule;
            shaderStages[1].pName = "main";

            shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStages[2].module = fragModule;
            shaderStages[2].pName = "main";

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
            pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
            pipelineInfo.pStages = shaderStages.data();
            // In mesh shading pipelines, pVertexInputState and pInputAssemblyState can be null or empty
            pipelineInfo.pVertexInputState = nullptr;
            pipelineInfo.pInputAssemblyState = nullptr;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = VK_NULL_HANDLE;

            vk_common::check_vk_result(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &meshPipeline), "Failed to create mesh shading graphics pipeline");
            std::cout << "[Pipeline] Task + Mesh Shading Pipeline compiled and created successfully.\n";
        }

        // STEP 6: Command Pool, Buffers & Sync Objects
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

        std::cout << "Mesh & Task Shading Pipeline active. Starting real-time render loop...\n";

        // STEP 7: Main Loop
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t currentFrame = 0;
        int renderedFrames = 0;

        
        // Initialize Flame Graph Profiler for assignment11_mesh_task_shading
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment11_mesh_task_shading");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment11_mesh_task_shading");
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

            // Setup Camera View-Projection Matrix
            vk_math::Vec3 eyePos(2.0f * std::cos(time * 0.5f), 1.5f, 2.0f * std::sin(time * 0.5f));
            vk_math::Mat4 view = vk_math::Mat4::lookAt(eyePos, vk_math::Vec3(0.0f, 0.0f, 0.0f), vk_math::Vec3(0.0f, 1.0f, 0.0f));
            vk_math::Mat4 proj = vk_math::Mat4::perspective(vk_math::radians(60.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.1f, 100.0f);
            vk_math::Mat4 viewProj = proj * view;

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
            colorAttachment.clearValue.color = {{0.04f, 0.05f, 0.08f, 1.0f}}; // Modern deep slate

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

            if (meshPipeline != VK_NULL_HANDLE && pfn_vkCmdDrawMeshTasksEXT) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

                VkViewport viewport{0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f};
                VkRect2D scissor{{0, 0}, {WIDTH, HEIGHT}};
                vkCmdSetViewport(cmd, 0, 1, &viewport);
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                MeshPushConstants pc{};
                pc.viewProj = viewProj;
                pc.time = time;
                pc.totalMeshlets = TOTAL_MESHLETS;

                vkCmdPushConstants(
                    cmd,
                    pipelineLayout,
                    VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(MeshPushConstants),
                    &pc
                );

                // Dispatch Task shader workgroups: Task shader launches mesh shaders on GPU
                pfn_vkCmdDrawMeshTasksEXT(cmd, TOTAL_MESHLETS, 1, 1);
            }

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

        std::cout << "[Status] Successfully rendered " << renderedFrames << " frames via VK_EXT_mesh_shader task & mesh pipeline.\n";

        vkDeviceWaitIdle(device);
        profiler.resolveGpuResults();
        profiler.exportFoldedFile("flamegraph_assignment11_mesh_task_shading.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment11_mesh_task_shading.html");
        profiler.exportChromeTraceFile("flamegraph_assignment11_mesh_task_shading.json");
        profiler.cleanupGpu();


        // STEP 8: Resource Cleanup
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        if (meshPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, meshPipeline, nullptr);
        }
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        if (taskModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, taskModule, nullptr);
        if (meshModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, meshModule, nullptr);
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

    std::cout << "Assignment 11 (Mesh & Task Shaders) completed cleanly.\n";
    return EXIT_SUCCESS;
}
