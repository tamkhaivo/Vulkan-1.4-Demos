// ============================================================================
// Vulkan 1.4 Common Framework Header
// Standardized for Clang 17+ Compiler and Vulkan 1.4 API Specification
// ============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <string>
#include "vulkan_flame_graph.hpp"

namespace vk_common {

// Helper to assert VkResult status
inline void check_vk_result(VkResult err, const char* msg) {
    if (err != VK_SUCCESS) {
        std::cerr << "[Vulkan 1.4 Error] " << msg << " Error code: " << err << std::endl;
        throw std::runtime_error(msg);
    }
}

} // namespace vk_common

namespace vulkan_utils {

// Structure holding Vulkan 1.4 core function pointers
struct Vulkan14Functions {
    PFN_vkCmdBeginRendering vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering vkCmdEndRendering = nullptr;
    PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2 = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;

    void load(VkDevice device) {
        vkCmdBeginRendering = (PFN_vkCmdBeginRendering)vkGetDeviceProcAddr(device, "vkCmdBeginRendering");
        if (!vkCmdBeginRendering) {
            vkCmdBeginRendering = (PFN_vkCmdBeginRendering)vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
        }

        vkCmdEndRendering = (PFN_vkCmdEndRendering)vkGetDeviceProcAddr(device, "vkCmdEndRendering");
        if (!vkCmdEndRendering) {
            vkCmdEndRendering = (PFN_vkCmdEndRendering)vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");
        }

        vkCmdPipelineBarrier2 = (PFN_vkCmdPipelineBarrier2)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2");
        if (!vkCmdPipelineBarrier2) {
            vkCmdPipelineBarrier2 = (PFN_vkCmdPipelineBarrier2)vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2KHR");
        }

        vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress");
        if (!vkGetBufferDeviceAddress) {
            vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
        }
    }
};

// Create GLFW Window for Vulkan
inline GLFWwindow* createWindow(int width = 800, int height = 600, const char* title = "Vulkan 1.4 Assignment") {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    return window;
}

// Create Vulkan 1.4 Instance
inline VkInstance createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan 1.4 Core Framework";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 4, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 4, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4; // Target Vulkan 1.4 Specification

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool validationSupported = true;
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            validationSupported = false;
            break;
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (validationSupported) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkInstance instance;
    vk_common::check_vk_result(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create Vulkan 1.4 VkInstance");
    return instance;
}

// Find Vulkan 1.4 Physical Device
inline VkPhysicalDevice findPhysicalDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return device;
        }
    }
    return devices[0];
}

// Find Graphics Queue Family
inline uint32_t findGraphicsQueueFamily(VkPhysicalDevice physicalDevice) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find graphics queue family");
}

// Create Logical Device targeting Vulkan 1.4 with core Dynamic Rendering, Synchronization2, and Buffer Device Address
inline VkDevice createDevice(VkPhysicalDevice physicalDevice, uint32_t& graphicsQueueFamily) {
    graphicsQueueFamily = findGraphicsQueueFamily(physicalDevice);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Vulkan 1.2 Core Features (Buffer Device Address & Timeline Semaphores)
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.timelineSemaphore = VK_TRUE;
    vulkan12Features.descriptorIndexing = VK_TRUE;

    // Vulkan 1.3 Core Features (Dynamic Rendering & Synchronization2)
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &vulkan12Features;
    vulkan13Features.dynamicRendering = VK_TRUE;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.maintenance4 = VK_TRUE;

    // Vulkan 1.4 Core Features (Vertex Attribute Divisor, Push Descriptors, etc.)
    VkPhysicalDeviceVulkan14Features vulkan14Features{};
    vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    vulkan14Features.pNext = &vulkan13Features;
    vulkan14Features.vertexAttributeInstanceRateDivisor = VK_TRUE;
    vulkan14Features.vertexAttributeInstanceRateZeroDivisor = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &vulkan14Features;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures2.features.multiDrawIndirect = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device;
    vk_common::check_vk_result(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create Vulkan 1.4 logical device");
    return device;
}

inline VkDevice createDevice(VkPhysicalDevice physicalDevice) {
    uint32_t dummyFamily;
    return createDevice(physicalDevice, dummyFamily);
}

// Create Surface via GLFW
inline VkSurfaceKHR getSurface(VkInstance instance, GLFWwindow* window) {
    VkSurfaceKHR surface;
    vk_common::check_vk_result(glfwCreateWindowSurface(instance, window, nullptr, &surface), "Failed to create window surface");
    return surface;
}

// Helper to read binary SPIR-V shader files
inline std::vector<char> readFile(const std::string& filename) {
    std::vector<std::string> candidates = {
        filename,
        "assignment01_hello_triangle/" + filename,
        "assignment02_rotating_cube/" + filename,
        "assignment03_textured_quad/" + filename,
        "assignment04_push_constants_dynamic_uniforms/" + filename,
        "assignment05_instanced_rendering/" + filename,
        "assignment06_two_pass_dynamic_rendering_local_read/" + filename,
        "assignment07_compute_particles_indirect_draw/" + filename,
        "assignment08_deferred_shading_g_buffer/" + filename,
        "assignment09_multithreaded_command_recording/" + filename,
        "assignment10_buffer_device_address_streaming/" + filename,
        "assignment11_mesh_task_shading/" + filename,
        "assignment12_descriptor_buffers/" + filename,
        "assignment13_pipeline_binaries_cache/" + filename,
        "assignment14_subgroup_arithmetic_reduction/" + filename,
        "assignment15_ray_queries_inline/" + filename,
        "assignment16_gpu_driven_draw_indirect_count/" + filename,
        "assignment17_shader_objects/" + filename,
        "assignment18_hardware_ray_tracing_pipeline/" + filename,
        "assignment19_variable_rate_shading/" + filename,
        "assignment20_sparse_virtual_texturing/" + filename,
        "assignment21_bindless_texturing/" + filename,
        "assignment22_async_compute_transfer_overlap/" + filename,
        "assignment23_conditional_rendering_occlusion_queries/" + filename,
        "assignment24_dynamic_rendering_msaa_resolve/" + filename,
        "assignment25_clustered_forward_lighting/" + filename,
        "assignment26_device_generated_commands/" + filename,
        "assignment27_extended_dynamic_state3/" + filename,
        "assignment28_calibrated_timestamps_gpu_profiling/" + filename,
        "assignment29_host_image_copy/" + filename,
        "assignment30_mesh_shading_culling_lod/" + filename,
        "assignment31_maintenance5_maintenance6/" + filename,
        "assignment32_opacity_micromaps/" + filename,
        "assignment33_subgroup_partitioned_quad/" + filename,
        "assignment34_dynamic_rendering_suspend_resume/" + filename,
        "assignment35_shader_execution_reordering/" + filename,
        "assignment36_ray_tracing_motion_blur/" + filename,
        "assignment37_cooperative_matrix/" + filename,
        "assignment38_vulkan_memory_model/" + filename,
        "assignment39_displacement_micromaps/" + filename,
        "assignment40_dynamic_rendering_unused_attachments/" + filename,
        "assignment41_multiview_stereo_vr/" + filename,
        "assignment42_custom_border_color_sampler/" + filename,
        "assignment43_cluster_acceleration_structure/" + filename,
        "assignment44_external_memory_interop/" + filename,
        "assignment45_pipeline_robustness_fault_tolerance/" + filename,
        "assignment46_push_descriptors/" + filename,
        "assignment47_multiview_mesh_shading/" + filename,
        "assignment48_timeline_semaphore_batch_graph/" + filename,
        "assignment49_low_latency_swapchain_timing/" + filename,
        "assignment50_ray_tracing_callable_shaders/" + filename,
        "assignment51_dma_sparse_residency_streaming/" + filename,
        "assignment52_cooperative_vector_tensor_filtering/" + filename,
        "assignment53_ray_tracing_position_fetch/" + filename,
        "assignment54_device_diagnostic_checkpoints/" + filename,
        "assignment55_saliency_shading_rate_maps/" + filename,
        "assignment56_dgc_token_multi_pipeline_draws/" + filename,
        "assignment57_hdr_color_space_metadata/" + filename,
        "assignment58_vulkan_video_hardware_decode/" + filename,
        "assignment59_memory_model_queue_transfers/" + filename,
        "assignment60_indirect_ray_tracing_dispatch/" + filename,
        "assignment61_ray_tracing_motion_blur_matrices/" + filename,
        "assignment62_subgroup_cluster_operations/" + filename,
        "assignment63_multi_draw_indirect_draw_parameters/" + filename,
        "assignment64_present_timing_frame_pacing/" + filename,
        "assignment65_async_compute_physics_graphics/" + filename,
        "assignment66_rasterization_order_subpass_shading/" + filename,
        "assignment67_mesh_shading_multi_topologies/" + filename,
        "assignment68_custom_gpu_memory_allocator/" + filename,
        "assignment69_acceleration_structure_compaction_serialization/" + filename,
        "assignment70_autonomous_gpu_driven_engine/" + filename
    };

    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();
            return buffer;
        }
    }
    throw std::runtime_error("Failed to open shader file: " + filename);
}

// Create Shader Module
inline VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    vk_common::check_vk_result(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module");
    return shaderModule;
}

// Helper for Vulkan 1.4 Synchronization2 Image Layout Transitions
inline void pipelineBarrier2ImageTransition(
    VkCommandBuffer cmdBuffer,
    PFN_vkCmdPipelineBarrier2 cmdBarrier2,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 dstAccess,
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    cmdBarrier2(cmdBuffer, &dependencyInfo);
}

} // namespace vulkan_utils
