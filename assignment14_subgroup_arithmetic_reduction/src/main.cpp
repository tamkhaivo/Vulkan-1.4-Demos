// ============================================================================
// Assignment 14: Subgroup Operations & Wave-Level Math
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - Subgroup basic & arithmetic operations (subgroupAdd, subgroupElect, ballot)
//   - Bank-conflict-free parallel compute reduction
//   - Vulkan 1.4 Core Synchronization2 and SSBO data streaming
//   - Real GPU Compute Pipeline execution and verification
// ============================================================================

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <array>
#include <numeric>
#include <cstring>
#include <filesystem>
#include "vulkan_common.hpp"

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

void createBuffer(
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

int main() {
    std::cout << "========================================================\n";
    std::cout << "Assignment 14: Subgroup Operations & Wave-Level Math\n";
    std::cout << "Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << "Concepts: GL_KHR_shader_subgroup_arithmetic, SIMD Lane Reductions,\n";
    std::cout << "          subgroupAdd / subgroupElect, Compute Parallel Reductions\n";
    std::cout << "========================================================\n";

    try {
        // 1. Create Vulkan 1.4 Instance
        VkInstance instance = vulkan_utils::createInstance();
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // 2. Query Physical Device & Subgroup Properties
        VkPhysicalDeviceSubgroupProperties subgroupProperties{};
        subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        VkPhysicalDeviceProperties2 properties2{};
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &subgroupProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

        std::cout << "Hardware Device: " << properties2.properties.deviceName << "\n";
        std::cout << "Hardware Subgroup Size: " << subgroupProperties.subgroupSize << " SIMD lanes.\n";
        std::cout << "Supported Subgroup Operations: ";
        if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) std::cout << "Basic ";
        if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) std::cout << "Arithmetic ";
        if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) std::cout << "Ballot ";
        if (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) std::cout << "Shuffle ";
        std::cout << "\n";

        if (!(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)) {
            std::cerr << "Warning: VK_SUBGROUP_FEATURE_ARITHMETIC_BIT not supported on this device!\n";
        }

        // 3. Create Logical Device targeting Compute
        uint32_t queueFamily = UINT32_MAX;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queueFamily = i;
                break;
            }
        }

        if (queueFamily == UINT32_MAX) {
            throw std::runtime_error("No compute queue family found!");
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &vulkan13Features;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        VkDevice device = VK_NULL_HANDLE;
        vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device");

        VkQueue queue;
        vkGetDeviceQueue(device, queueFamily, 0, &queue);

        vulkan_utils::Vulkan14Functions vk14{};
        vk14.load(device);

        // 4. Command Pool & Command Buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamily;

        VkCommandPool commandPool;
        vk_common::check_vk_result(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool");

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vk_common::check_vk_result(vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer), "Failed to allocate command buffer");

        // 5. Allocate Input and Output SSBO Buffers
        // Input: 1024 floats with values 1.0, 2.0, 3.0, ..., 1024.0
        const uint32_t numElements = 1024;
        const uint32_t localSizeX = 64; // defined in subgroup_reduce.comp
        const uint32_t numWorkGroups = (numElements + localSizeX - 1) / localSizeX; // 16 workgroups

        VkDeviceSize inputBufferSize = sizeof(float) * numElements;
        VkDeviceSize outputBufferSize = sizeof(float) * numWorkGroups;

        std::vector<float> hostInputData(numElements);
        float expectedSum = 0.0f;
        for (uint32_t i = 0; i < numElements; ++i) {
            hostInputData[i] = static_cast<float>(i + 1);
            expectedSum += hostInputData[i];
        }

        VkBuffer inputBuffer, outputBuffer;
        VkDeviceMemory inputBufferMemory, outputBufferMemory;

        createBuffer(device, physicalDevice, inputBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     inputBuffer, inputBufferMemory);

        createBuffer(device, physicalDevice, outputBufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     outputBuffer, outputBufferMemory);

        // Upload host input data
        void* mappedInput = nullptr;
        vkMapMemory(device, inputBufferMemory, 0, inputBufferSize, 0, &mappedInput);
        std::memcpy(mappedInput, hostInputData.data(), (size_t)inputBufferSize);
        vkUnmapMemory(device, inputBufferMemory);

        // Zero out output buffer
        void* mappedOutput = nullptr;
        vkMapMemory(device, outputBufferMemory, 0, outputBufferSize, 0, &mappedOutput);
        std::memset(mappedOutput, 0, (size_t)outputBufferSize);
        vkUnmapMemory(device, outputBufferMemory);

        // 6. Descriptor Set Layout & Descriptor Pool
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        // Binding 0: inData (InputBuffer)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        // Binding 1: outData (OutputBuffer)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        vk_common::check_vk_result(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout), "Failed to create descriptor set layout");

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.pPoolSizes = &poolSize;
        poolCreateInfo.maxSets = 1;

        VkDescriptorPool descriptorPool;
        vk_common::check_vk_result(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create descriptor pool");

        VkDescriptorSetAllocateInfo allocDescInfo{};
        allocDescInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocDescInfo.descriptorPool = descriptorPool;
        allocDescInfo.descriptorSetCount = 1;
        allocDescInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vk_common::check_vk_result(vkAllocateDescriptorSets(device, &allocDescInfo, &descriptorSet), "Failed to allocate descriptor set");

        VkDescriptorBufferInfo inBufferInfoDesc{};
        inBufferInfoDesc.buffer = inputBuffer;
        inBufferInfoDesc.offset = 0;
        inBufferInfoDesc.range = inputBufferSize;

        VkDescriptorBufferInfo outBufferInfoDesc{};
        outBufferInfoDesc.buffer = outputBuffer;
        outBufferInfoDesc.offset = 0;
        outBufferInfoDesc.range = outputBufferSize;

        std::array<VkWriteDescriptorSet, 2> writeSets{};
        writeSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSets[0].dstSet = descriptorSet;
        writeSets[0].dstBinding = 0;
        writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSets[0].descriptorCount = 1;
        writeSets[0].pBufferInfo = &inBufferInfoDesc;

        writeSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSets[1].dstSet = descriptorSet;
        writeSets[1].dstBinding = 1;
        writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSets[1].descriptorCount = 1;
        writeSets[1].pBufferInfo = &outBufferInfoDesc;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);

        // 7. Pipeline Layout & Compute Pipeline
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        vk_common::check_vk_result(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");

        std::string compPath = "shaders/subgroup_reduce.comp.spv";
        if (!std::filesystem::exists(compPath)) compPath = "assignment14_subgroup_arithmetic_reduction/shaders/subgroup_reduce.comp.spv";
        auto compCode = vulkan_utils::readFile(compPath);
        VkShaderModule compModule = vulkan_utils::createShaderModule(device, compCode);

        VkComputePipelineCreateInfo compPipelineInfo{};
        compPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compPipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compModule, "main", nullptr};
        compPipelineInfo.layout = pipelineLayout;

        VkPipeline computePipeline;
        vk_common::check_vk_result(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compPipelineInfo, nullptr, &computePipeline), "Failed to create compute pipeline");

        // 8. Record & Submit Compute Command Buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDispatch(commandBuffer, numWorkGroups, 1, 1);

        // Compute write -> Host read barrier using Synchronization2
        VkBufferMemoryBarrier2 bufferBarrier{};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        bufferBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = outputBuffer;
        bufferBarrier.offset = 0;
        bufferBarrier.size = outputBufferSize;

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = 1;
        dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;

        if (vk14.vkCmdPipelineBarrier2) {
            vk14.vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        } else {
            VkBufferMemoryBarrier legacyBarrier{};
            legacyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            legacyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            legacyBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            legacyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            legacyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            legacyBarrier.buffer = outputBuffer;
            legacyBarrier.offset = 0;
            legacyBarrier.size = outputBufferSize;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 0, 0, nullptr, 1, &legacyBarrier, 0, nullptr);
        }

        vkEndCommandBuffer(commandBuffer);

        std::cout << "Input Array Size: " << numElements << " floats (values: 1.0f .. " << numElements << ".0f).\n";
        std::cout << "[Compute Dispatch] Executing subgroupAdd() parallel reduction pass on GPU (" << numWorkGroups << " workgroups)...\n";

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        // 9. Read Back and Verify GPU Output
        float* outputResult = nullptr;
        vkMapMemory(device, outputBufferMemory, 0, outputBufferSize, 0, reinterpret_cast<void**>(&outputResult));

        float totalGpuSum = 0.0f;
        for (uint32_t i = 0; i < numWorkGroups; ++i) {
            totalGpuSum += outputResult[i];
        }
        vkUnmapMemory(device, outputBufferMemory);

        std::cout << "[GPU Verification] Expected Sum: " << expectedSum
                  << " | Subgroup-reduced Sum: " << totalGpuSum;
        if (std::abs(totalGpuSum - expectedSum) < 1e-3f) {
            std::cout << " (Exact match - SUCCESS!)\n";
        } else {
            std::cout << " (MISMATCH! Error)\n";
        }

        std::cout << "Assignment 14 (Subgroup Operations & Wave-Level Math) completed cleanly.\n";

        // Cleanup
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyShaderModule(device, compModule, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyBuffer(device, inputBuffer, nullptr);
        vkFreeMemory(device, inputBufferMemory, nullptr);
        vkDestroyBuffer(device, outputBuffer, nullptr);
        vkFreeMemory(device, outputBufferMemory, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

