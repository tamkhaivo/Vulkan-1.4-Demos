// ============================================================================
// Assignment 21: Bindless Texturing & Non-Uniform Resource Indexing
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
//   - descriptorBindingPartiallyBound & runtimeDescriptorArray features
//   - shaderSampledImageArrayNonUniformIndexing & GL_EXT_nonuniform_qualifier
//   - Massive unbounded texture arrays (sampler2D globalTextures[])
//   - Dynamic per-draw/per-instance texture indexing via Push Constants
// ============================================================================

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <cmath>
#include "vulkan_common.hpp"
#include "vulkan_math.hpp"

struct PushConstants {
    vk_math::Mat4 mvp;
    uint32_t textureIndex;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 21: Bindless Texturing (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: descriptorBindingPartiallyBound, Non-Uniform Indexing,\n";
    std::cout << "           Unbounded Sampler Arrays, Push Constant Texture IDs\n";
    std::cout << "========================================================\n";

    try {
        VkInstance instance = vulkan_utils::createInstance();
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // Check Descriptor Indexing features (Vulkan 1.2+ Core / Vulkan 1.4)
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &indexingFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

        std::cout << "Hardware Device: " << deviceProps.deviceName << "\n";
        std::cout << "\n--- Descriptor Indexing & Bindless Feature Support ---\n";
        std::cout << "  - shaderSampledImageArrayNonUniformIndexing: " 
                  << (indexingFeatures.shaderSampledImageArrayNonUniformIndexing ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - descriptorBindingPartiallyBound:          " 
                  << (indexingFeatures.descriptorBindingPartiallyBound ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - descriptorBindingVariableDescriptorCount: " 
                  << (indexingFeatures.descriptorBindingVariableDescriptorCount ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - runtimeDescriptorArray:                   " 
                  << (indexingFeatures.runtimeDescriptorArray ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - descriptorBindingSampledImageUpdateAfterBind: " 
                  << (indexingFeatures.descriptorBindingSampledImageUpdateAfterBind ? "SUPPORTED" : "UNSUPPORTED") << "\n";

        // Query physical device limits for descriptor indexing
        VkPhysicalDeviceDescriptorIndexingProperties indexingProps{};
        indexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

        VkPhysicalDeviceProperties2 deviceProps2{};
        deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProps2.pNext = &indexingProps;

        vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);

        std::cout << "\n--- Bindless Descriptor Capacity Limits ---\n";
        std::cout << "  - Max Descriptor Set UpdateAfterBind Samplers: " 
                  << indexingProps.maxDescriptorSetUpdateAfterBindSampledImages << "\n";
        std::cout << "  - Max Per-Stage Descriptor UpdateAfterBind Samplers: " 
                  << indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages << "\n";

        // Create Logical Device enabling bindless features
        uint32_t queueFamilyIndex = 0;
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceDescriptorIndexingFeatures enabledIndexingFeatures{};
        enabledIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        enabledIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        enabledIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        enabledIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        enabledIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        enabledIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &enabledIndexingFeatures;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device with bindless descriptor indexing!");
        }

        std::cout << "[Device] Successfully created Vulkan 1.4 Device with Bindless Descriptor Indexing.\n";

        // 1. Create Descriptor Set Layout with Unbounded Variable-Count Array
        const uint32_t MAX_TEXTURE_SLOTS = 1024;

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = MAX_TEXTURE_SLOTS;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags bindingFlags = 
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = 1;
        bindingFlagsInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create bindless descriptor set layout!");
        }

        std::cout << "[Layout] Created Bindless DescriptorSetLayout (" << MAX_TEXTURE_SLOTS << " variable-count slots).\n";

        // 2. Create Descriptor Pool for Update-After-Bind
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = MAX_TEXTURE_SLOTS;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create update-after-bind descriptor pool!");
        }

        // 3. Allocate Variable-Count Descriptor Set
        uint32_t variableCount = MAX_TEXTURE_SLOTS;
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocInfo{};
        variableCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableCountAllocInfo.descriptorSetCount = 1;
        variableCountAllocInfo.pDescriptorCounts = &variableCount;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = &variableCountAllocInfo;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet bindlessDescriptorSet;
        if (vkAllocateDescriptorSets(device, &allocInfo, &bindlessDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate bindless descriptor set!");
        }

        std::cout << "[Descriptor Set] Successfully allocated bindless descriptor set.\n";
        std::cout << "                 Partially bound: Slots can be populated on-demand without binding whole table.\n";
        std::cout << "                 Shaders dynamically index via nonuniformEXT(pushConstant.textureIndex).\n";

        // Cleanup
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);

        std::cout << "\nAssignment 21 execution & verification passed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 21 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
