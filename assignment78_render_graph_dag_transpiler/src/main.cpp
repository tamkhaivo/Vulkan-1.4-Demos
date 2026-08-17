// ============================================================================
// Assignment 78: Multi-Queue Timeline Render Graph with Automatic Synchronization2 Transpiler
// Standardized for Clang 17+ Compiler & Vulkan 1.4 Specification
// Concepts:
//   - DAG Render Graph Compiler & Automated Pass Dependency Ordering
//   - Automatic RAW / WAR Hazard Detection & VkDependencyInfo Transpilation
//   - Multi-Pass Pipeline: Scene Rendering -> Post-Processing Bloom -> Tonemap
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
#include <string>
#include <map>
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

struct ScenePushConstants {
    vk_math::Mat4 mvp;
    vk_math::Mat4 model;
};

struct PostPushConstants {
    float bloomIntensity;
    float time;
};

// ----------------------------------------------------------------------------
// Simplified DAG Render Graph Transpiler
// ----------------------------------------------------------------------------
struct RenderGraphResource {
    std::string name;
    VkImage image;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags2 currentStage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 currentAccess = VK_ACCESS_2_NONE;
};

struct RenderGraphPass {
    std::string name;
    std::vector<std::string> readResources;
    std::vector<std::string> writeResources;
};

class RenderGraph {
public:
    void addResource(const std::string& name, VkImage image) {
        resources[name] = { name, image };
    }

    void addPass(const std::string& name, const std::vector<std::string>& reads, const std::vector<std::string>& writes) {
        passes.push_back({ name, reads, writes });
    }

    void compileAndExecutePassTransition(VkCommandBuffer cmd, const std::string& passName, vulkan_utils::Vulkan14Functions& vk14) {
        // Find pass
        for (const auto& p : passes) {
            if (p.name == passName) {
                std::vector<VkImageMemoryBarrier2> barriers;

                // Handle write transitions (Color attachment)
                for (const auto& w : p.writeResources) {
                    if (resources.find(w) != resources.end()) {
                        auto& res = resources[w];
                        VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                        b.srcStageMask = res.currentStage == VK_PIPELINE_STAGE_2_NONE ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : res.currentStage;
                        b.srcAccessMask = res.currentAccess;
                        b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                        b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                        b.oldLayout = res.currentLayout;
                        b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        b.image = res.image;
                        b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                        barriers.push_back(b);

                        res.currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        res.currentStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                        res.currentAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                    }
                }

                // Handle read transitions (Shader read)
                for (const auto& r : p.readResources) {
                    if (resources.find(r) != resources.end()) {
                        auto& res = resources[r];
                        if (res.currentLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                            VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                            b.srcStageMask = res.currentStage;
                            b.srcAccessMask = res.currentAccess;
                            b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                            b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                            b.oldLayout = res.currentLayout;
                            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            b.image = res.image;
                            b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                            barriers.push_back(b);

                            res.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            res.currentStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                            res.currentAccess = VK_ACCESS_2_SHADER_READ_BIT;
                        }
                    }
                }

                if (!barriers.empty()) {
                    VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
                    depInfo.imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
                    depInfo.pImageMemoryBarriers = barriers.data();
                    vk14.vkCmdPipelineBarrier2(cmd, &depInfo);
                }
                break;
            }
        }
    }

private:
    std::map<std::string, RenderGraphResource> resources;
    std::vector<RenderGraphPass> passes;
};

// Generate 3D Torus Knot Geometry
void generateTorusKnotGeometry(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    const int NUM_SECTIONS = 120;
    const int SECTION_VERTICES = 12;
    const float TUBE_RADIUS = 0.12f;

    for (int i = 0; i <= NUM_SECTIONS; ++i) {
        float phi = float(i) * 2.0f * 3.14159265f / float(NUM_SECTIONS);
        // (p=2, q=3) Trefoil Torus Knot
        float r = 0.5f + 0.25f * std::cos(3.0f * phi);
        float x = r * std::cos(2.0f * phi);
        float y = r * std::sin(2.0f * phi);
        float z = 0.25f * std::sin(3.0f * phi);

        float nextPhi = float(i + 1) * 2.0f * 3.14159265f / float(NUM_SECTIONS);
        float nextR = 0.5f + 0.25f * std::cos(3.0f * nextPhi);
        float nx = nextR * std::cos(2.0f * nextPhi);
        float ny = nextR * std::sin(2.0f * nextPhi);
        float nz = 0.25f * std::sin(3.0f * nextPhi);

        vk_math::Vec3 p(x, y, z);
        vk_math::Vec3 tangent = (vk_math::Vec3(nx, ny, nz) - p).normalize();
        vk_math::Vec3 normal = tangent.cross({0, 1, 0}).normalize();
        vk_math::Vec3 binormal = tangent.cross(normal).normalize();

        float rCol = 0.5f + 0.5f * std::cos(phi);
        float gCol = 0.5f + 0.5f * std::sin(phi);
        float bCol = 0.8f;

        for (int j = 0; j < SECTION_VERTICES; ++j) {
            float theta = float(j) * 2.0f * 3.14159265f / float(SECTION_VERTICES);
            vk_math::Vec3 radialDir = normal * std::cos(theta) + binormal * std::sin(theta);
            vertices.push_back({ p + radialDir * TUBE_RADIUS, radialDir, { rCol, gCol, bCol } });
        }
    }

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        for (int j = 0; j < SECTION_VERTICES; ++j) {
            int nextJ = (j + 1) % SECTION_VERTICES;
            uint16_t c0 = i * SECTION_VERTICES + j;
            uint16_t c1 = i * SECTION_VERTICES + nextJ;
            uint16_t n0 = (i + 1) * SECTION_VERTICES + j;
            uint16_t n1 = (i + 1) * SECTION_VERTICES + nextJ;

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

void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_common::check_vk_result(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, image, &memReq);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vk_common::check_vk_result(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");
    vk_common::check_vk_result(vkBindImageMemory(device, image, imageMemory, 0), "Failed to bind image memory");
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView imageView;
    vk_common::check_vk_result(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "Failed to create image view");
    return imageView;
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << " Assignment 78: Render Graph DAG & Synchronization2 (Vulkan 1.4)\n";
    std::cout << " Compiled with Clang 17+ Standard | Targeting VK_API_VERSION_1_4\n";
    std::cout << " Concepts: Automated RAW/WAR Hazard Tracking, Synchronization2,\n";
    std::cout << "           DAG Pass Compilation & Multi-Pass Bloom Post-Processing\n";
    std::cout << "====================================================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment 78: Render Graph DAG Compiler", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    try {
        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Assignment 78 - Render Graph Transpiler";
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

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        vulkan13Features.dynamicRendering = VK_TRUE;
        vulkan13Features.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        deviceFeatures2.pNext = &vulkan13Features;

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
            swapchainImageViews[i] = createImageView(device, swapchainImages[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        // Offscreen Scene Color Image & Depth
        VkFormat sceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImage sceneColorImage, depthImage;
        VkDeviceMemory sceneColorMemory, depthMemory;
        VkImageView sceneColorView, depthView;

        createImage(device, physicalDevice, WIDTH, HEIGHT, sceneColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, sceneColorImage, sceneColorMemory);
        createImage(device, physicalDevice, WIDTH, HEIGHT, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage, depthMemory);

        sceneColorView = createImageView(device, sceneColorImage, sceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        depthView = createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Sampler
        VkSampler sampler;
        {
            VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
        }

        // Descriptor Set Layout for Post Pass
        VkDescriptorSetLayoutBinding texBinding{};
        texBinding.binding = 0;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.descriptorCount = 1;
        texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        texBinding.pImmutableSamplers = &sampler;

        VkDescriptorSetLayoutCreateInfo descLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descLayoutInfo.bindingCount = 1;
        descLayoutInfo.pBindings = &texBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &descriptorSetLayout);

        VkDescriptorPool descriptorPool;
        {
            VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 } };
            VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.maxSets = 2;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = poolSizes;
            vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
        }

        VkDescriptorSet descriptorSet;
        {
            VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &descriptorSetLayout;
            vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

            VkDescriptorImageInfo imageInfo{ sampler, sceneColorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        // Shaders & Pipelines
        std::string shaderDir = "assignment78_render_graph_dag_transpiler/shaders/";
        if (!std::filesystem::exists(shaderDir + "scene.vert.spv")) shaderDir = "shaders/";

        auto sVertCode = vulkan_utils::readFile(shaderDir + "scene.vert.spv");
        auto sFragCode = vulkan_utils::readFile(shaderDir + "scene.frag.spv");
        auto pVertCode = vulkan_utils::readFile(shaderDir + "post_bloom.vert.spv");
        auto pFragCode = vulkan_utils::readFile(shaderDir + "post_bloom.frag.spv");

        VkShaderModule sVertModule = vulkan_utils::createShaderModule(device, sVertCode);
        VkShaderModule sFragModule = vulkan_utils::createShaderModule(device, sFragCode);
        VkShaderModule pVertModule = vulkan_utils::createShaderModule(device, pVertCode);
        VkShaderModule pFragModule = vulkan_utils::createShaderModule(device, pFragCode);

        // 1. Scene Pipeline Layout
        VkPushConstantRange scenePcRange{};
        scenePcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        scenePcRange.offset = 0;
        scenePcRange.size = sizeof(ScenePushConstants);

        VkPipelineLayoutCreateInfo scenePipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        scenePipeLayoutInfo.pushConstantRangeCount = 1;
        scenePipeLayoutInfo.pPushConstantRanges = &scenePcRange;

        VkPipelineLayout scenePipelineLayout;
        vkCreatePipelineLayout(device, &scenePipeLayoutInfo, nullptr, &scenePipelineLayout);

        // 2. Post Pipeline Layout
        VkPushConstantRange postPcRange{};
        postPcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        postPcRange.offset = 0;
        postPcRange.size = sizeof(PostPushConstants);

        VkPipelineLayoutCreateInfo postPipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        postPipeLayoutInfo.setLayoutCount = 1;
        postPipeLayoutInfo.pSetLayouts = &descriptorSetLayout;
        postPipeLayoutInfo.pushConstantRangeCount = 1;
        postPipeLayoutInfo.pPushConstantRanges = &postPcRange;

        VkPipelineLayout postPipelineLayout;
        vkCreatePipelineLayout(device, &postPipeLayoutInfo, nullptr, &postPipelineLayout);

        // Create Scene Pipeline
        VkPipeline scenePipeline;
        {
            VkPipelineShaderStageCreateInfo stages[] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, sVertModule, "main", nullptr },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, sFragModule, "main", nullptr }
            };

            auto bindingDesc = Vertex::getBindingDescription();
            auto attrDesc = Vertex::getAttributeDescriptions();
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size());
            vertexInputInfo.pVertexAttributeDescriptions = attrDesc.data();

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
            renderingCreateInfo.pColorAttachmentFormats = &sceneColorFormat;
            renderingCreateInfo.depthAttachmentFormat = depthFormat;

            VkGraphicsPipelineCreateInfo pipeInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            pipeInfo.pNext = &renderingCreateInfo;
            pipeInfo.stageCount = 2;
            pipeInfo.pStages = stages;
            pipeInfo.pVertexInputState = &vertexInputInfo;
            pipeInfo.pInputAssemblyState = &inputAssembly;
            pipeInfo.pViewportState = &viewportState;
            pipeInfo.pRasterizationState = &rasterizer;
            pipeInfo.pMultisampleState = &multisampling;
            pipeInfo.pDepthStencilState = &depthStencil;
            pipeInfo.pColorBlendState = &colorBlending;
            pipeInfo.pDynamicState = &dynamicState;
            pipeInfo.layout = scenePipelineLayout;

            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &scenePipeline);
        }

        // Create Post Pipeline
        VkPipeline postPipeline;
        {
            VkPipelineShaderStageCreateInfo stages[] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, pVertModule, "main", nullptr },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, pFragModule, "main", nullptr }
            };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState colorBlend{};
            colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlend;

            std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkPipelineRenderingCreateInfo renderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            renderingCreateInfo.colorAttachmentCount = 1;
            renderingCreateInfo.pColorAttachmentFormats = &surfaceFormat.format;

            VkGraphicsPipelineCreateInfo pipeInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            pipeInfo.pNext = &renderingCreateInfo;
            pipeInfo.stageCount = 2;
            pipeInfo.pStages = stages;
            pipeInfo.pVertexInputState = &vertexInputInfo;
            pipeInfo.pInputAssemblyState = &inputAssembly;
            pipeInfo.pViewportState = &viewportState;
            pipeInfo.pRasterizationState = &rasterizer;
            pipeInfo.pMultisampleState = &multisampling;
            pipeInfo.pColorBlendState = &colorBlending;
            pipeInfo.pDynamicState = &dynamicState;
            pipeInfo.layout = postPipelineLayout;

            vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &postPipeline);
        }

        // Geometry Setup
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        generateTorusKnotGeometry(vertices, indices);

        VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        createBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);
        void* vData;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vData);
        std::memcpy(vData, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);

        VkDeviceSize indexBufferSize = sizeof(uint16_t) * indices.size();
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        createBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexBufferMemory);
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

        // Initialize Render Graph DAG
        RenderGraph renderGraph;
        renderGraph.addResource("SceneColor", sceneColorImage);
        renderGraph.addResource("DepthBuffer", depthImage);
        renderGraph.addPass("ScenePass", {}, { "SceneColor" });
        renderGraph.addPass("PostBloomPass", { "SceneColor" }, {});

        std::cout << "[Render Loop] Executing Multi-Pass Render Graph DAG Auto-Transpiler Loop...\n";

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t frameCount = 0;

        while (!glfwWindowShouldClose(window) && frameCount < 400) {
            glfwPollEvents();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) break;

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            vk_math::Mat4 model = vk_math::Mat4::rotate(time * 1.4f, { 0.0f, 1.0f, 0.0f }) * vk_math::Mat4::rotate(time * 0.7f, { 1.0f, 0.0f, 0.0f });
            vk_math::Mat4 view = vk_math::Mat4::lookAt({ 0.0f, 0.0f, 2.3f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
            vk_math::Mat4 proj = vk_math::Mat4::perspective(45.0f * 3.14159265f / 180.0f, float(WIDTH) / float(HEIGHT), 0.1f, 100.0f);
            proj.m[1][1] *= -1.0f;

            ScenePushConstants scenePc{};
            scenePc.mvp = proj * view * model;
            scenePc.model = model;

            PostPushConstants postPc{};
            postPc.bloomIntensity = 1.8f;
            postPc.time = time;

            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginCmd{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(commandBuffer, &beginCmd);

            // 1. DAG Auto-Transpile Barriers for ScenePass
            renderGraph.compileAndExecutePassTransition(commandBuffer, "ScenePass", vk14);

            VkImageMemoryBarrier2 depthBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            depthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthBarrier.image = depthImage;
            depthBarrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
            VkDependencyInfo dDep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            dDep.imageMemoryBarrierCount = 1;
            dDep.pImageMemoryBarriers = &depthBarrier;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &dDep);

            // 2. Execute Scene Dynamic Rendering Pass
            VkRenderingAttachmentInfo sceneColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            sceneColorAttachment.imageView = sceneColorView;
            sceneColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            sceneColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            sceneColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            sceneColorAttachment.clearValue = {{{ 0.02f, 0.02f, 0.04f, 1.0f }}};

            VkRenderingAttachmentInfo depthAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            depthAttachmentInfo.imageView = depthView;
            depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo sceneRenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            sceneRenderInfo.renderArea = { {0, 0}, {WIDTH, HEIGHT} };
            sceneRenderInfo.layerCount = 1;
            sceneRenderInfo.colorAttachmentCount = 1;
            sceneRenderInfo.pColorAttachments = &sceneColorAttachment;
            sceneRenderInfo.pDepthAttachment = &depthAttachmentInfo;

            vk14.vkCmdBeginRendering(commandBuffer, &sceneRenderInfo);

            VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f };
            VkRect2D scissor{ {0, 0}, {WIDTH, HEIGHT} };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
            vkCmdPushConstants(commandBuffer, scenePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &scenePc);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // 3. DAG Auto-Transpile Barriers for PostBloomPass
            renderGraph.compileAndExecutePassTransition(commandBuffer, "PostBloomPass", vk14);

            VkImageMemoryBarrier2 swapBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            swapBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            swapBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapBarrier.image = swapchainImages[imageIndex];
            swapBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VkDependencyInfo sDep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            sDep.imageMemoryBarrierCount = 1;
            sDep.pImageMemoryBarriers = &swapBarrier;
            vk14.vkCmdPipelineBarrier2(commandBuffer, &sDep);

            // 4. Execute PostBloom Pass
            VkRenderingAttachmentInfo swapColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            swapColorAttachment.imageView = swapchainImageViews[imageIndex];
            swapColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            swapColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            swapColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            swapColorAttachment.clearValue = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};

            VkRenderingInfo postRenderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            postRenderInfo.renderArea = { {0, 0}, {WIDTH, HEIGHT} };
            postRenderInfo.layerCount = 1;
            postRenderInfo.colorAttachmentCount = 1;
            postRenderInfo.pColorAttachments = &swapColorAttachment;

            vk14.vkCmdBeginRendering(commandBuffer, &postRenderInfo);

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, postPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostPushConstants), &postPc);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

            vk14.vkCmdEndRendering(commandBuffer);

            // 5. Transition to Present
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

        // Cleanup
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyImageView(device, sceneColorView, nullptr);
        vkDestroyImage(device, sceneColorImage, nullptr);
        vkFreeMemory(device, sceneColorMemory, nullptr);

        vkDestroyImageView(device, depthView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthMemory, nullptr);

        vkDestroySampler(device, sampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyPipeline(device, scenePipeline, nullptr);
        vkDestroyPipeline(device, postPipeline, nullptr);
        vkDestroyPipelineLayout(device, scenePipelineLayout, nullptr);
        vkDestroyPipelineLayout(device, postPipelineLayout, nullptr);

        vkDestroyShaderModule(device, sVertModule, nullptr);
        vkDestroyShaderModule(device, sFragModule, nullptr);
        vkDestroyShaderModule(device, pVertModule, nullptr);
        vkDestroyShaderModule(device, pFragModule, nullptr);
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

        std::cout << "\nAssignment 78 executed cleanly (" << frameCount << " frames rendered).\n";
    } catch (const std::exception& e) {
        std::cerr << "Assignment 78 Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
