// ============================================================================
// Assignment 20: Sparse / Virtual Texturing & Mip-Level Streaming
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - VK_IMAGE_CREATE_SPARSE_BINDING_BIT & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
//   - vkGetPhysicalDeviceSparseImageFormatProperties
//   - vkGetImageSparseMemoryRequirements & Sparse image tile granularity
//   - vkQueueBindSparse memory tile management & Mip-tail buffer packing
//   - Minimal VRAM footprint with massive virtual textures
// ============================================================================

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
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

int main() {
    std::cout << "========================================================\n";
    std::cout << " Assignment 20: Sparse Virtual Texturing (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Sparse Image Residency, Tile Granularity,\n";
    std::cout << "           Mip-Tail Memory Management, vkQueueBindSparse\n";
    std::cout << "========================================================\n";

    try {
        // 1. Create Vulkan 1.4 Instance
        VkInstance instance = vulkan_utils::createInstance();
        VkPhysicalDevice physicalDevice = vulkan_utils::findPhysicalDevice(instance);

        // 2. Query Physical Device Sparse Features & Properties
        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

        VkPhysicalDeviceProperties deviceProps{};
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);

        std::cout << "Hardware Device: " << deviceProps.deviceName << "\n";
        std::cout << "\n--- Sparse Resource Feature Support ---\n";
        std::cout << "  - sparseBinding:                 " << (supportedFeatures.sparseBinding ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidencyBuffer:         " << (supportedFeatures.sparseResidencyBuffer ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidencyImage2D:        " << (supportedFeatures.sparseResidencyImage2D ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidencyImage3D:        " << (supportedFeatures.sparseResidencyImage3D ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidency2Samples:       " << (supportedFeatures.sparseResidency2Samples ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidency4Samples:       " << (supportedFeatures.sparseResidency4Samples ? "SUPPORTED" : "UNSUPPORTED") << "\n";
        std::cout << "  - sparseResidencyAliased:        " << (supportedFeatures.sparseResidencyAliased ? "SUPPORTED" : "UNSUPPORTED") << "\n";

        // Query sparse image format properties for standard 2D RGBA8 texture
        VkFormat testFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageType testType = VK_IMAGE_TYPE_2D;
        VkSampleCountFlagBits testSamples = VK_SAMPLE_COUNT_1_BIT;
        VkImageUsageFlags testUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageTiling testTiling = VK_IMAGE_TILING_OPTIMAL;

        uint32_t sparseFormatPropCount = 0;
        vkGetPhysicalDeviceSparseImageFormatProperties(
            physicalDevice,
            testFormat,
            testType,
            testSamples,
            testUsage,
            testTiling,
            &sparseFormatPropCount,
            nullptr
        );

        std::vector<VkSparseImageFormatProperties> sparseFormatProps(sparseFormatPropCount);
        if (sparseFormatPropCount > 0) {
            vkGetPhysicalDeviceSparseImageFormatProperties(
                physicalDevice,
                testFormat,
                testType,
                testSamples,
                testUsage,
                testTiling,
                &sparseFormatPropCount,
                sparseFormatProps.data()
            );

            std::cout << "\n--- Sparse Image Format Properties (VK_FORMAT_R8G8B8A8_UNORM) ---\n";
            for (size_t i = 0; i < sparseFormatProps.size(); ++i) {
                const auto& prop = sparseFormatProps[i];
                std::cout << "  Config #" << i << ":\n";
                std::cout << "    Tile Granularity (WxHxD): "
                          << prop.imageGranularity.width << " x "
                          << prop.imageGranularity.height << " x "
                          << prop.imageGranularity.depth << " texels\n";
                std::cout << "    Aspect Mask: 0x" << std::hex << prop.aspectMask << std::dec << "\n";
                std::cout << "    Flags: Single Mip Tail = "
                          << ((prop.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) ? "YES" : "NO")
                          << ", Aligned Mip Size = "
                          << ((prop.flags & VK_SPARSE_IMAGE_FORMAT_ALIGNED_MIP_SIZE_BIT) ? "YES" : "NO")
                          << ", Nonstandard Block Size = "
                          << ((prop.flags & VK_SPARSE_IMAGE_FORMAT_NONSTANDARD_BLOCK_SIZE_BIT) ? "YES" : "NO")
                          << "\n";
            }
        }

        // Find queue families with sparse binding and graphics support
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t sparseQueueIndex = UINT32_MAX;
        uint32_t graphicsQueueIndex = UINT32_MAX;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) && (sparseQueueIndex == UINT32_MAX)) {
                sparseQueueIndex = i;
            }
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (graphicsQueueIndex == UINT32_MAX)) {
                graphicsQueueIndex = i;
            }
        }

        std::cout << "\nQueue Family Selection:\n";
        std::cout << "  - Graphics Queue Family: " << (graphicsQueueIndex != UINT32_MAX ? std::to_string(graphicsQueueIndex) : "None") << "\n";
        std::cout << "  - Sparse Queue Family:   " << (sparseQueueIndex != UINT32_MAX ? std::to_string(sparseQueueIndex) : "None") << "\n";

        // 3. Create Logical Device with sparse binding feature enabled if supported
        VkDevice device = VK_NULL_HANDLE;
        VkQueue sparseQueue = VK_NULL_HANDLE;

        if (supportedFeatures.sparseBinding && supportedFeatures.sparseResidencyImage2D && sparseQueueIndex != UINT32_MAX) {
            float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = sparseQueueIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkPhysicalDeviceFeatures enabledFeatures{};
            enabledFeatures.sparseBinding = VK_TRUE;
            enabledFeatures.sparseResidencyImage2D = VK_TRUE;

            VkDeviceCreateInfo deviceCreateInfo{};
            deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreateInfo.queueCreateInfoCount = 1;
            deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
            deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

            vk_common::check_vk_result(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device");
            vkGetDeviceQueue(device, sparseQueueIndex, 0, &sparseQueue);

            std::cout << "\n--- Sparse Virtual Image Creation Test ---\n";
            const uint32_t virtualWidth = 8192;
            const uint32_t virtualHeight = 8192;
            const uint32_t mipLevels = 14;

            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.extent.width = virtualWidth;
            imageInfo.extent.height = virtualHeight;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

            VkImage sparseImage = VK_NULL_HANDLE;
            vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &sparseImage), "Failed to create sparse image");

            // Query standard memory requirements
            VkMemoryRequirements memReqs{};
            vkGetImageMemoryRequirements(device, sparseImage, &memReqs);
            std::cout << "Virtual Image: " << virtualWidth << "x" << virtualHeight << " (" << mipLevels << " mips)\n";
            std::cout << "Theoretical full VRAM allocation size: " << (memReqs.size / (1024 * 1024)) << " MB\n";
            std::cout << "Memory Type Bits: 0x" << std::hex << memReqs.memoryTypeBits << std::dec << "\n";

            // Query sparse memory requirements (tiles & mip tails)
            uint32_t sparseReqCount = 0;
            vkGetImageSparseMemoryRequirements(device, sparseImage, &sparseReqCount, nullptr);
            std::vector<VkSparseImageMemoryRequirements> sparseReqs(sparseReqCount);
            vkGetImageSparseMemoryRequirements(device, sparseImage, &sparseReqCount, sparseReqs.data());

            std::cout << "Sparse Memory Requirement Blocks: " << sparseReqCount << "\n";
            for (uint32_t r = 0; r < sparseReqCount; ++r) {
                const auto& req = sparseReqs[r];
                std::cout << "  Requirement #" << r << ":\n";
                std::cout << "    Tile Granularity: " << req.formatProperties.imageGranularity.width
                          << " x " << req.formatProperties.imageGranularity.height
                          << " x " << req.formatProperties.imageGranularity.depth << " texels\n";
                std::cout << "    Image Mip Tail First LOD: " << req.imageMipTailFirstLod << "\n";
                std::cout << "    Image Mip Tail Size:       " << (req.imageMipTailSize / 1024) << " KB\n";
                std::cout << "    Image Mip Tail Offset:     " << req.imageMipTailOffset << "\n";
                std::cout << "    Image Mip Tail Stride:     " << req.imageMipTailStride << "\n";
            }

            // Allocate and bind the Mip-Tail memory so lower mips are always resident
            if (!sparseReqs.empty() && sparseReqs[0].imageMipTailSize > 0) {
                const auto& tailReq = sparseReqs[0];
                VkMemoryAllocateInfo tailAllocInfo{};
                tailAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                tailAllocInfo.allocationSize = tailReq.imageMipTailSize;
                tailAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

                VkDeviceMemory tailMemory = VK_NULL_HANDLE;
                vk_common::check_vk_result(vkAllocateMemory(device, &tailAllocInfo, nullptr, &tailMemory), "Failed to allocate mip tail memory");

                // Bind Mip Tail via vkQueueBindSparse
                VkSparseMemoryBind opaqueBind{};
                opaqueBind.resourceOffset = tailReq.imageMipTailOffset;
                opaqueBind.size = tailReq.imageMipTailSize;
                opaqueBind.memory = tailMemory;
                opaqueBind.memoryOffset = 0;
                opaqueBind.flags = 0;

                VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo{};
                opaqueBindInfo.image = sparseImage;
                opaqueBindInfo.bindCount = 1;
                opaqueBindInfo.pBinds = &opaqueBind;

                VkBindSparseInfo bindSparseInfo{};
                bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
                bindSparseInfo.imageOpaqueBindCount = 1;
                bindSparseInfo.pImageOpaqueBinds = &opaqueBindInfo;

                VkFence bindFence = VK_NULL_HANDLE;
                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                vkCreateFence(device, &fenceInfo, nullptr, &bindFence);

                vk_common::check_vk_result(vkQueueBindSparse(sparseQueue, 1, &bindSparseInfo, bindFence), "Failed to execute vkQueueBindSparse");
                vkWaitForFences(device, 1, &bindFence, VK_TRUE, UINT64_MAX);

                std::cout << "[vkQueueBindSparse] Successfully bound opaque mip-tail memory ("
                          << (tailReq.imageMipTailSize / 1024) << " KB) to GPU queue.\n";

                vkDestroyFence(device, bindFence, nullptr);
                vkFreeMemory(device, tailMemory, nullptr);
            }

            vkDestroyImage(device, sparseImage, nullptr);
            vkDestroyDevice(device, nullptr);
        } else {
            std::cout << "\n[Note] Sparse residency not fully supported on this device/queue; capability queries succeeded.\n";
        }

        vkDestroyInstance(instance, nullptr);
        std::cout << "\nAssignment 20 execution & verification passed successfully.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

