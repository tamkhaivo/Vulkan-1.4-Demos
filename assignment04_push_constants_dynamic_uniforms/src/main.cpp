// ============================================================================
// Assignment 4: Push Constants and Dynamic Uniform Buffers
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// ============================================================================

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "vulkan_common.hpp"

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Assignment 4: Push Constants & Dynamic Uniforms (Vulkan 1.4)" << std::endl;
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4" << std::endl;
    std::cout << "Concepts: VkPushConstantRange, Dynamic Offsets, Vulkan 1.4 Pipeline Layout" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        GLFWwindow* window = vulkan_utils::createWindow(800, 600, "Assignment 4: Push Constants (Vulkan 1.4)");
        VkInstance instance = vulkan_utils::createInstance();
        VkSurfaceKHR surface = vulkan_utils::getSurface(instance, window);
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);
        
        uint32_t graphicsQueueFamily = UINT32_MAX;
        VkDevice device = vulkan_utils::createDevice(physicalDevice, graphicsQueueFamily);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        std::cout << "Vulkan 1.4 Logical Device initialized for Assignment 4." << std::endl;

        // Clean shutdown
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "[Exception] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Assignment 4 initialized cleanly under Vulkan 1.4 standard." << std::endl;
    return EXIT_SUCCESS;
}
