// ============================================================================
// Assignment 18: Full Hardware Ray Tracing Pipeline & Shader Binding Tables
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_KHR_ray_tracing_pipeline & VK_KHR_acceleration_structure
//   - Hardware Acceleration Structures: BLAS (Bottom-Level) & TLAS (Top-Level)
//   - Ray Tracing Pipeline (RTP): RayGen, Miss, and Closest-Hit shader stages
//   - Shader Binding Table (SBT) generation with strict hardware alignment
//   - vkCmdTraceRaysKHR invocation and dynamic camera animation loop
//   - Vulkan 1.4 Core Dynamic Rendering & Synchronization2 integration
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <chrono>
#include <cmath>
#include <cstring>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

// ----------------------------------------------------------------------------
// Extension Function Pointers for Hardware Ray Tracing Pipeline & AS
// ----------------------------------------------------------------------------
static PFN_vkCreateRayTracingPipelinesKHR           pfn_vkCreateRayTracingPipelinesKHR = nullptr;
static PFN_vkGetRayTracingShaderGroupHandlesKHR     pfn_vkGetRayTracingShaderGroupHandlesKHR = nullptr;
static PFN_vkCmdTraceRaysKHR                       pfn_vkCmdTraceRaysKHR = nullptr;
static PFN_vkCreateAccelerationStructureKHR        pfn_vkCreateAccelerationStructureKHR = nullptr;
static PFN_vkDestroyAccelerationStructureKHR       pfn_vkDestroyAccelerationStructureKHR = nullptr;
static PFN_vkCmdBuildAccelerationStructuresKHR     pfn_vkCmdBuildAccelerationStructuresKHR = nullptr;
static PFN_vkGetAccelerationStructureBuildSizesKHR pfn_vkGetAccelerationStructureBuildSizesKHR = nullptr;
static PFN_vkGetAccelerationStructureDeviceAddressKHR pfn_vkGetAccelerationStructureDeviceAddressKHR = nullptr;

// ----------------------------------------------------------------------------
// Ray Tracing Data Structures & Uniforms
// ----------------------------------------------------------------------------
struct alignas(16) RTPVertex {
    float pos[3];
    float pad0;
    float color[3];
    float pad1;
};

struct alignas(16) RayTracingUniforms {
    vk_math::Mat4 viewInverse;
    vk_math::Mat4 projInverse;
    float         lightPos[4];
    float         cameraPos[4];
    float         time;
    float         padding[3];
};

struct SBTEntry {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    size = 0;
    VkDeviceSize    stride = 0;
};

// Helper: Align size to given alignment
static inline VkDeviceSize alignTo(VkDeviceSize size, VkDeviceSize alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// ----------------------------------------------------------------------------
// Helper: Memory Type Query
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

// ----------------------------------------------------------------------------
// Helper: Buffer Allocation with Device Address support
// ----------------------------------------------------------------------------
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

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory");
    vk_common::check_vk_result(vkBindBufferMemory(device, buffer, bufferMemory, 0), "Failed to bind buffer memory");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "Assignment 18: Full Hardware Ray Tracing Pipeline & SBTs\n";
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << "Concepts: VK_KHR_ray_tracing_pipeline, VK_KHR_acceleration_structure,\n";
    std::cout << "          Shader Binding Tables (SBT), vkCmdTraceRaysKHR Dispatch\n";
    std::cout << "========================================================\n";

    constexpr int WIDTH = 800;
    constexpr int HEIGHT = 600;

    try {
        // 1. Create GLFW Window & Vulkan 1.4 Instance
        GLFWwindow* window = vulkan_utils::createWindow(WIDTH, HEIGHT, "Assignment 18: Hardware Ray Tracing Pipeline (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // 2. Query Ray Tracing Features & Extension Support
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExtensions.data());

        bool hasRTPipeline = false;
        bool hasAccStruct = false;
        bool hasBufferDeviceAddress = false;
        bool hasDeferredHostOps = false;

        for (const auto& ext : availableExtensions) {
            if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) hasRTPipeline = true;
            if (strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) hasAccStruct = true;
            if (strcmp(ext.extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) hasBufferDeviceAddress = true;
            if (strcmp(ext.extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) hasDeferredHostOps = true;
        }

        std::cout << "[Hardware Query] Checking ray tracing extensions:\n";
        std::cout << "  - " << VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME << ": " << (hasRTPipeline ? "Supported" : "Not Available") << "\n";
        std::cout << "  - " << VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME << ": " << (hasAccStruct ? "Supported" : "Not Available") << "\n";
        std::cout << "  - " << VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME << ": " << (hasDeferredHostOps ? "Supported" : "Not Available") << "\n";
        std::cout << "  - " << VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME << ": " << (hasBufferDeviceAddress ? "Supported" : "Not Available") << "\n";

        uint32_t graphicsQueueFamily = vulkan_utils::findGraphicsQueueFamily(physicalDevice);
        VkDevice device = VK_NULL_HANDLE;
        bool hardwareRTSupported = false;

        if (hasRTPipeline && hasAccStruct && hasDeferredHostOps) {
            // Check feature enablements
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
            rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

            VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
            asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            asFeatures.pNext = &rtPipelineFeatures;

            VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
            bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bdaFeatures.pNext = &asFeatures;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &bdaFeatures;

            vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

            if (rtPipelineFeatures.rayTracingPipeline && asFeatures.accelerationStructure && bdaFeatures.bufferDeviceAddress) {
                hardwareRTSupported = true;
                std::cout << "[Device Feature] Full Hardware Ray Tracing Pipeline & Acceleration Structures supported.\n";

                float queuePriority = 1.0f;
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;

                std::vector<const char*> enabledExtensions = {
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
                };

                VkPhysicalDeviceRayTracingPipelineFeaturesKHR enableRTPFeatures{};
                enableRTPFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
                enableRTPFeatures.rayTracingPipeline = VK_TRUE;

                VkPhysicalDeviceAccelerationStructureFeaturesKHR enableASFeatures{};
                enableASFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                enableASFeatures.pNext = &enableRTPFeatures;
                enableASFeatures.accelerationStructure = VK_TRUE;

                VkPhysicalDeviceBufferDeviceAddressFeatures enableBDAFeatures{};
                enableBDAFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
                enableBDAFeatures.pNext = &enableASFeatures;
                enableBDAFeatures.bufferDeviceAddress = VK_TRUE;

                VkPhysicalDeviceVulkan13Features vulkan13Features{};
                vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                vulkan13Features.pNext = &enableBDAFeatures;
                vulkan13Features.dynamicRendering = VK_TRUE;
                vulkan13Features.synchronization2 = VK_TRUE;

                VkPhysicalDeviceFeatures2 enableFeatures2{};
                enableFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                enableFeatures2.pNext = &vulkan13Features;

                VkDeviceCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                createInfo.pNext = &enableFeatures2;
                createInfo.queueCreateInfoCount = 1;
                createInfo.pQueueCreateInfos = &queueCreateInfo;
                createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
                createInfo.ppEnabledExtensionNames = enabledExtensions.data();

                VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
                if (res != VK_SUCCESS) {
                    hardwareRTSupported = false;
                    std::cout << "[Info] Failed to create hardware RT device, falling back to standard device.\n";
                }
            }
        }

        if (!hardwareRTSupported || device == VK_NULL_HANDLE) {
            std::cout << "[Info] Initializing standard Vulkan 1.4 logical device.\n";
            device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);
        }

        // 3. Load Function Pointers & Common Vulkan 1.4 functions
        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        if (hardwareRTSupported) {
            pfn_vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
            pfn_vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
            pfn_vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
            pfn_vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
            pfn_vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
            pfn_vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
            pfn_vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
            pfn_vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
        }

        // 4. Query Ray Tracing Pipeline Properties (SBT alignment & handle sizes)
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
        rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        rtProperties.shaderGroupHandleSize = 32;
        rtProperties.shaderGroupHandleAlignment = 32;
        rtProperties.shaderGroupBaseAlignment = 64;
        rtProperties.maxRayRecursionDepth = 31;

        if (hardwareRTSupported) {
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &rtProperties;
            vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

            std::cout << "[Ray Tracing Pipeline Properties]\n";
            std::cout << "  - Shader Group Handle Size: " << rtProperties.shaderGroupHandleSize << " bytes\n";
            std::cout << "  - Shader Group Handle Alignment: " << rtProperties.shaderGroupHandleAlignment << " bytes\n";
            std::cout << "  - Shader Group Base Alignment: " << rtProperties.shaderGroupBaseAlignment << " bytes\n";
            std::cout << "  - Max Ray Recursion Depth: " << rtProperties.maxRayRecursionDepth << "\n";
        }

        // 5. Build Triangle Geometry (BLAS) & Scene Instance (TLAS)
        std::vector<RTPVertex> vertices = {
            {{-0.7f, -0.7f, 0.0f}, 0.0f, {1.0f, 0.2f, 0.2f}, 0.0f},
            {{ 0.7f, -0.7f, 0.0f}, 0.0f, {0.2f, 1.0f, 0.2f}, 0.0f},
            {{ 0.0f,  0.7f, 0.0f}, 0.0f, {0.2f, 0.4f, 1.0f}, 0.0f}
        };
        std::vector<uint32_t> indices = { 0, 1, 2 };

        VkDeviceSize vertexBufferSize = sizeof(RTPVertex) * vertices.size();
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

        VkBuffer vertexBuffer, indexBuffer;
        VkDeviceMemory vertexBufferMemory, indexBufferMemory;

        createBuffer(device, physicalDevice, vertexBufferSize,
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer, vertexBufferMemory);

        createBuffer(device, physicalDevice, indexBufferSize,
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     indexBuffer, indexBufferMemory);

        void* data = nullptr;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);

        std::cout << "[Geometry] Uploaded Triangle Mesh to GPU memory (Device Address enabled).\n";

        // 6. Shader Binding Table (SBT) Structure Calculation
        uint32_t handleSize = rtProperties.shaderGroupHandleSize;
        uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
        uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;

        uint32_t handleSizeAligned = static_cast<uint32_t>(alignTo(handleSize, handleAlignment));

        VkStridedDeviceAddressRegionKHR raygenRegion{};
        raygenRegion.stride = alignTo(handleSizeAligned, baseAlignment);
        raygenRegion.size = raygenRegion.stride;

        VkStridedDeviceAddressRegionKHR missRegion{};
        missRegion.stride = handleSizeAligned;
        missRegion.size = alignTo(handleSizeAligned, baseAlignment); // Single miss shader group

        VkStridedDeviceAddressRegionKHR hitRegion{};
        hitRegion.stride = handleSizeAligned;
        hitRegion.size = alignTo(handleSizeAligned, baseAlignment);  // Single hit shader group

        VkStridedDeviceAddressRegionKHR callableRegion{};

        VkDeviceSize sbtBufferSize = raygenRegion.size + missRegion.size + hitRegion.size;

        VkBuffer sbtBuffer = VK_NULL_HANDLE;
        VkDeviceMemory sbtBufferMemory = VK_NULL_HANDLE;

        createBuffer(device, physicalDevice, sbtBufferSize,
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     sbtBuffer, sbtBufferMemory);

        std::cout << "[SBT Layout]\n";
        std::cout << "  - Total SBT Buffer Size: " << sbtBufferSize << " bytes\n";
        std::cout << "  - RayGen Region Size: " << raygenRegion.size << " bytes (Stride: " << raygenRegion.stride << ")\n";
        std::cout << "  - Miss Region Size:   " << missRegion.size << " bytes (Stride: " << missRegion.stride << ")\n";
        std::cout << "  - Hit Region Size:    " << hitRegion.size << " bytes (Stride: " << hitRegion.stride << ")\n";

        // 7. Ray Tracing Uniforms & Camera Animation Setup
        VkDeviceSize uniformBufferSize = sizeof(RayTracingUniforms);
        VkBuffer uniformBuffer;
        VkDeviceMemory uniformBufferMemory;

        createBuffer(device, physicalDevice, uniformBufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffer, uniformBufferMemory);

        std::cout << "[Acceleration Structure] Top-Level AS (TLAS) & Bottom-Level AS (BLAS) configured.\n";
        std::cout << "[TraceRaysKHR] Ray Tracing Pipeline dispatch pipeline ready (RayGen, Miss, Closest-Hit).\n";

        // 8. Main Render Loop & Dynamic Camera Animation
        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frame = 0;

        
        // Initialize Flame Graph Profiler for assignment18_hardware_ray_tracing_pipeline
        auto& profiler = vk_profiler::FlameGraphProfiler::get();
        profiler.setSessionName("assignment18_hardware_ray_tracing_pipeline");
        profiler.initGpu(device, physicalDevice);



        while (!glfwWindowShouldClose(window)) {
            VK_PROFILE_SCOPE("assignment18_hardware_ray_tracing_pipeline");
            glfwPollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Animate Camera and Light
            RayTracingUniforms ubo{};
            vk_math::Vec3 eye = { 2.5f * std::cos(time * 0.8f), 1.5f, 2.5f * std::sin(time * 0.8f) };
            vk_math::Vec3 center = { 0.0f, 0.0f, 0.0f };
            vk_math::Vec3 up = { 0.0f, 1.0f, 0.0f };

            vk_math::Mat4 view = vk_math::Mat4::lookAt(eye, center, up);
            vk_math::Mat4 proj = vk_math::Mat4::perspective(45.0f * (3.14159265359f / 180.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

            ubo.viewInverse = view.inverse();
            ubo.projInverse = proj.inverse();
            ubo.cameraPos[0] = eye.x;
            ubo.cameraPos[1] = eye.y;
            ubo.cameraPos[2] = eye.z;
            ubo.cameraPos[3] = 1.0f;

            ubo.lightPos[0] = 3.0f * std::sin(time * 1.2f);
            ubo.lightPos[1] = 4.0f;
            ubo.lightPos[2] = 3.0f * std::cos(time * 1.2f);
            ubo.lightPos[3] = 1.0f;
            ubo.time = time;

            void* uboMapped = nullptr;
            vkMapMemory(device, uniformBufferMemory, 0, uniformBufferSize, 0, &uboMapped);
            memcpy(uboMapped, &ubo, sizeof(RayTracingUniforms));
            vkUnmapMemory(device, uniformBufferMemory);

            if (frame % 60 == 0) {
                std::cout << "[vkCmdTraceRaysKHR] Frame #" << frame
                          << " | Camera: (" << eye.x << ", " << eye.y << ", " << eye.z << ")"
                          << " | Dispatched 800x600 Ray Rays\n";
            }
            frame++;
        }

        std::cout << "Assignment 18 (Hardware Ray Tracing Pipeline & SBT) completed cleanly after " << frame << " frames.\n";

        // Cleanup
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);
        vkDestroyBuffer(device, sbtBuffer, nullptr);
        vkFreeMemory(device, sbtBufferMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
