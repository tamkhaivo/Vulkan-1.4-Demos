// ============================================================================
// Assignment 71: Nanite-Style Micro-Polygon Software Rasterizer via 64-bit Atomics
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_KHR_shader_atomic_int64 & 64-bit atomic visibility buffer
//   - Compute shader 2D edge-equation software micro-triangle rasterization
//   - Screen-space visibility resolve dynamic rendering pass
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
#include <fstream>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct alignas(16) MicroTriangle {
    Vec4 v0; // xy = screen pos (pixels), z = depth [0..1], w = 1
    Vec4 v1;
    Vec4 v2;
    uint32_t triId;
    uint32_t colorId;
    uint32_t pad0;
    uint32_t pad1;
};

struct RasterPushConstants {
    uint32_t numTriangles;
    int32_t screenWidth;
    int32_t screenHeight;
    float time;
};

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << " Assignment 71: Nanite-Style Software Rasterizer (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: VK_KHR_shader_atomic_int64, 64-bit Packed VisBuffer,\n";
    std::cout << "           Compute Micro-Rasterization, Dynamic Rendering Resolve\n";
    std::cout << "====================================================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment 71: Nanite Software Rasterizer (Vulkan 1.4)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    try {
        // 1. Vulkan 1.4 Instance Creation
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Assignment 71 - Nanite Software Rasterizer";
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
        if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }

        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }

        // 2. Physical Device Selection
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for (const auto& dev : physicalDevices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (props.apiVersion >= VK_API_VERSION_1_3) {
                physicalDevice = dev;
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            physicalDevice = physicalDevices[0];
        }

        // 3. Queue Family Selection
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && presentSupport) {
                graphicsQueueFamilyIndex = i;
                break;
            }
        }

        // 4. Device Features Setup (Vulkan 1.4 Dynamic Rendering & 64-bit Atomics)
        VkPhysicalDeviceShaderAtomicInt64Features atomic64Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES };
        atomic64Features.shaderBufferInt64Atomics = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;
        vulkan13Features.pNext = &atomic64Features;

        VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        vulkan12Features.bufferDeviceAddress = VK_TRUE;
        vulkan12Features.pNext = &vulkan13Features;

        VkPhysicalDeviceFeatures2 deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures2.features.shaderInt64 = VK_TRUE;
        deviceFeatures2.pNext = &vulkan12Features;

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        deviceCreateInfo.pNext = &deviceFeatures2;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device with 64-bit atomics!");
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

        // Load Vulkan 1.4 Dynamic Function Pointers
        vulkan_utils::Vulkan14Functions vk14;
        vk14.load(device);

        // 5. Swapchain Setup
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
        VkExtent2D swapExtent = { WIDTH, HEIGHT };

        VkSwapchainCreateInfoKHR swapInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        swapInfo.surface = surface;
        swapInfo.minImageCount = 2;
        swapInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapInfo.imageExtent = swapExtent;
        swapInfo.imageArrayLayers = 1;
        swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapInfo.preTransform = capabilities.currentTransform;
        swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapInfo.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        if (vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain!");
        }

        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapchainImages(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        std::vector<VkImageView> swapchainImageViews(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain image view!");
            }
        }

        // Memory type helper
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        auto findMemoryType = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }
            throw std::runtime_error("Failed to find suitable memory type!");
        };

        // 6. 64-Bit Visibility Buffer (SSBO) & Micro-Triangles Buffer
        const size_t numPixels = WIDTH * HEIGHT;
        const VkDeviceSize visBufferSize = numPixels * sizeof(uint64_t);

        VkBuffer visBuffer;
        VkDeviceMemory visBufferMemory;
        {
            VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufInfo.size = visBufferSize;
            bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(device, &bufInfo, nullptr, &visBuffer);

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, visBuffer, &memReq);

            VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &visBufferMemory);
            vkBindBufferMemory(device, visBuffer, visBufferMemory, 0);
        }

        // Generate Dense Micro-Triangles Cluster (Nanite-Style Geometry)
        std::vector<MicroTriangle> cpuTriangles;
        const int GRID_RES_X = 120;
        const int GRID_RES_Y = 80;
        uint32_t currentTriId = 0;

        for (int y = 0; y < GRID_RES_Y; ++y) {
            for (int x = 0; x < GRID_RES_X; ++x) {
                float u0 = float(x) / float(GRID_RES_X);
                float u1 = float(x + 1) / float(GRID_RES_X);
                float v0 = float(y) / float(GRID_RES_Y);
                float v1 = float(y + 1) / float(GRID_RES_Y);

                float px0 = 100.0f + u0 * 1080.0f;
                float px1 = 100.0f + u1 * 1080.0f;
                float py0 = 80.0f + v0 * 560.0f;
                float py1 = 80.0f + v1 * 560.0f;

                float z00 = 0.5f + 0.3f * std::sin(u0 * 8.0f) * std::cos(v0 * 8.0f);
                float z10 = 0.5f + 0.3f * std::sin(u1 * 8.0f) * std::cos(v0 * 8.0f);
                float z01 = 0.5f + 0.3f * std::sin(u0 * 8.0f) * std::cos(v1 * 8.0f);
                float z11 = 0.5f + 0.3f * std::sin(u1 * 8.0f) * std::cos(v1 * 8.0f);

                // Tri 1
                MicroTriangle t1{};
                t1.v0 = Vec4(px0, py0, z00, 1.0f);
                t1.v1 = Vec4(px1, py0, z10, 1.0f);
                t1.v2 = Vec4(px0, py1, z01, 1.0f);
                t1.triId = currentTriId++;
                t1.colorId = t1.triId;
                cpuTriangles.push_back(t1);

                // Tri 2
                MicroTriangle t2{};
                t2.v0 = Vec4(px1, py0, z10, 1.0f);
                t2.v1 = Vec4(px1, py1, z11, 1.0f);
                t2.v2 = Vec4(px0, py1, z01, 1.0f);
                t2.triId = currentTriId++;
                t2.colorId = t2.triId;
                cpuTriangles.push_back(t2);
            }
        }

        const VkDeviceSize triBufferSize = cpuTriangles.size() * sizeof(MicroTriangle);
        VkBuffer triBuffer;
        VkDeviceMemory triBufferMemory;
        {
            VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufInfo.size = triBufferSize;
            bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(device, &bufInfo, nullptr, &triBuffer);

            VkMemoryRequirements memReq;
            vkGetBufferMemoryRequirements(device, triBuffer, &memReq);

            VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &triBufferMemory);
            vkBindBufferMemory(device, triBuffer, triBufferMemory, 0);

            void* data;
            vkMapMemory(device, triBufferMemory, 0, triBufferSize, 0, &data);
            std::memcpy(data, cpuTriangles.data(), triBufferSize);
            vkUnmapMemory(device, triBufferMemory);
        }

        // 7. Descriptor Set Layout & Pipeline Layouts
        VkDescriptorSetLayoutBinding triBinding{};
        triBinding.binding = 0;
        triBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        triBinding.descriptorCount = 1;
        triBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding visBinding{};
        visBinding.binding = 1;
        visBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        visBinding.descriptorCount = 1;
        visBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings = { triBinding, visBinding };
        VkDescriptorSetLayoutCreateInfo descLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descLayoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        descLayoutInfo.pBindings = layoutBindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &descriptorSetLayout);

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(RasterPushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        // Descriptor Pool & Set
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 }
        };
        VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.maxSets = 2;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = poolSizes;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

        VkDescriptorSetAllocateInfo allocSetInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocSetInfo.descriptorPool = descriptorPool;
        allocSetInfo.descriptorSetCount = 1;
        allocSetInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(device, &allocSetInfo, &descriptorSet);

        VkDescriptorBufferInfo triBufferInfo{ triBuffer, 0, triBufferSize };
        VkDescriptorBufferInfo visBufferInfo{ visBuffer, 0, visBufferSize };

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &triBufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptorSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &visBufferInfo;

        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

        // 8. Pipelines Creation (Compute Soft Rasterizer + Graphics Dynamic Rendering Resolve)
        std::string exeDir = "assignment71_nanite_software_rasterizer/shaders/";
        if (!std::filesystem::exists(exeDir + "soft_raster.comp.spv")) {
            exeDir = "shaders/";
        }

        auto compCode = readFile(exeDir + "soft_raster.comp.spv");
        VkShaderModule compModule = createShaderModule(device, compCode);

        VkComputePipelineCreateInfo computePipeInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        computePipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computePipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipeInfo.stage.module = compModule;
        computePipeInfo.stage.pName = "main";
        computePipeInfo.layout = pipelineLayout;

        VkPipeline computePipeline;
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipeInfo, nullptr, &computePipeline);

        // Resolve Graphics Pipeline
        auto vertCode = readFile(exeDir + "vis_resolve.vert.spv");
        auto fragCode = readFile(exeDir + "vis_resolve.frag.spv");
        VkShaderModule vertModule = createShaderModule(device, vertCode);
        VkShaderModule fragModule = createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr }
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

        VkGraphicsPipelineCreateInfo graphicsPipeInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        graphicsPipeInfo.pNext = &pipelineRenderingCreateInfo;
        graphicsPipeInfo.stageCount = 2;
        graphicsPipeInfo.pStages = shaderStages;
        graphicsPipeInfo.pVertexInputState = &vertexInputInfo;
        graphicsPipeInfo.pInputAssemblyState = &inputAssembly;
        graphicsPipeInfo.pViewportState = &viewportState;
        graphicsPipeInfo.pRasterizationState = &rasterizer;
        graphicsPipeInfo.pMultisampleState = &multisampling;
        graphicsPipeInfo.pColorBlendState = &colorBlending;
        graphicsPipeInfo.pDynamicState = &dynamicState;
        graphicsPipeInfo.layout = pipelineLayout;

        VkPipeline graphicsPipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipeInfo, nullptr, &graphicsPipeline);

        // 9. Command Pool & Command Buffer Setup
        VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

        VkCommandPool commandPool;
        vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
        VkFence inFlightFence;
        VkSemaphoreCreateInfo semaInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateSemaphore(device, &semaInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(device, &semaInfo, nullptr, &renderFinishedSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

        std::cout << "[Render Loop] Executing Vulkan 1.4 Nanite Software Rasterizer...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t frameCount = 0;

        
        // Initialize Flame Graph Profiler for assignment71_nanite_software_rasterizer
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment71_nanite_software_rasterizer");
        profiler.initGpu(device, physicalDevice);

        while (!glfwWindowShouldClose(window) && frameCount < 400) {
            VK_PROFILE_SCOPE("assignment71_nanite_software_rasterizer");
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Animate micro-triangles on CPU host buffer
            {
                void* data;
                vkMapMemory(device, triBufferMemory, 0, triBufferSize, 0, &data);
                auto* triPtr = reinterpret_cast<MicroTriangle*>(data);
                for (size_t i = 0; i < cpuTriangles.size(); ++i) {
                    MicroTriangle t = cpuTriangles[i];
                    float wave = std::sin(time * 2.5f + float(t.triId) * 0.005f) * 15.0f;
                    t.v0.y += wave;
                    t.v1.y += wave;
                    t.v2.y += wave;
                    triPtr[i] = t;
                }
                vkUnmapMemory(device, triBufferMemory);
            }

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // Phase 1: Clear 64-Bit Visibility Buffer with max uint64 (depth = 1.0, triId = 0xFFFFFFFF)
            // 0xFFFFFFFFFFFFFFFFu corresponds to depth = 1.0 (inf) and invalid primitive ID
            vkCmdFillBuffer(commandBuffer, visBuffer, 0, visBufferSize, 0xFFFFFFFF);

            // Barrier: Transfer Fill -> Compute Shader Read/Write
            VkBufferMemoryBarrier2 fillToComputeBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
            fillToComputeBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            fillToComputeBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            fillToComputeBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            fillToComputeBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            fillToComputeBarrier.buffer = visBuffer;
            fillToComputeBarrier.offset = 0;
            fillToComputeBarrier.size = VK_WHOLE_SIZE;

            VkDependencyInfo fillDepInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            fillDepInfo.bufferMemoryBarrierCount = 1;
            fillDepInfo.pBufferMemoryBarriers = &fillToComputeBarrier;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &fillDepInfo);

            // Phase 2: Compute Software Rasterizer Dispatch
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            RasterPushConstants pushConstants{};
            pushConstants.numTriangles = static_cast<uint32_t>(cpuTriangles.size());
            pushConstants.screenWidth = WIDTH;
            pushConstants.screenHeight = HEIGHT;
            pushConstants.time = time;
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RasterPushConstants), &pushConstants);

            uint32_t dispatchGroups = (pushConstants.numTriangles + 63) / 64;
            vkCmdDispatch(commandBuffer, dispatchGroups, 1, 1);

            // Barrier: Compute Shader Write -> Fragment Shader Read
            VkBufferMemoryBarrier2 computeToFragBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
            computeToFragBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            computeToFragBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            computeToFragBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            computeToFragBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            computeToFragBarrier.buffer = visBuffer;
            computeToFragBarrier.offset = 0;
            computeToFragBarrier.size = VK_WHOLE_SIZE;

            VkDependencyInfo computeToFragDepInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            computeToFragDepInfo.bufferMemoryBarrierCount = 1;
            computeToFragDepInfo.pBufferMemoryBarriers = &computeToFragBarrier;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &computeToFragDepInfo);

            // Phase 3: Transition Swapchain Image & Fullscreen Dynamic Rendering Resolve Pass
            VkImageMemoryBarrier2 barrierToColor{};
            barrierToColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrierToColor.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
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

            VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            colorAttachment.imageView = swapchainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue = {{{ 0.02f, 0.02f, 0.05f, 1.0f }}};

            VkRenderingInfo renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            renderInfo.renderArea = { {0, 0}, swapExtent };
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = 1;
            renderInfo.pColorAttachments = &colorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &renderInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ {0, 0}, swapExtent };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RasterPushConstants), &pushConstants);

            // Draw fullscreen triangle to resolve visibility buffer
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // Phase 4: Transition to Present
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
        profiler.exportFoldedFile("flamegraph_assignment71_nanite_software_rasterizer.folded");
        profiler.exportInteractiveHTML("flamegraph_assignment71_nanite_software_rasterizer.html");
        profiler.exportChromeTraceFile("flamegraph_assignment71_nanite_software_rasterizer.json");
        profiler.cleanupGpu();


        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, triBuffer, nullptr);
        vkFreeMemory(device, triBufferMemory, nullptr);
        vkDestroyBuffer(device, visBuffer, nullptr);
        vkFreeMemory(device, visBufferMemory, nullptr);

        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyShaderModule(device, compModule, nullptr);
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

        std::cout << "\nAssignment 71 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 71 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
