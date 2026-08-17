# Assignment 10 – Buffer Device Address and Zero-Copy Streaming

## Overview & Architectural Critique
Traditional descriptor-based buffer access (`VkDescriptorSet`) requires significant CPU overhead for descriptor pool allocation, descriptor set updates, and driver validation. Furthermore, it imposes strict limits on the number of buffers simultaneously accessible in shaders.

In Vulkan 1.4, **Buffer Device Address (BDA, `VK_KHR_buffer_device_address`)** enables shaders to directly dereference 64-bit GPU virtual memory pointers using `GL_EXT_buffer_reference2`. Combined with Resizable BAR (ReBAR) / host-visible device-local memory (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`), developers can achieve true zero-copy pointer streaming directly from CPU writes to GPU shader execution without staging copies.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core BDA Feature**: `VkPhysicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE`.
- **Buffer Usage Flag**: `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`.
- **Querying Device Address**: `vkGetBufferDeviceAddress(device, &addressInfo)` returning `VkDeviceAddress` (`uint64_t`).
- **GLSL Pointer Dereferencing**: `layout(buffer_reference, scalar) buffer NodeBlock { ... };` passing 64-bit addresses via Push Constants.

## Concrete Implementation Example (Vulkan 1.4 C++ & GLSL)

### C++ Host Code
```cpp
// 1. Create BDA-enabled Storage Buffer
VkBufferCreateInfo bufferInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = bufferSize,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
};
vkCreateBuffer(device, &bufferInfo, nullptr, &bdaBuffer);

// 2. Query 64-bit GPU Device Address
VkBufferDeviceAddressInfo addressInfo{
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = bdaBuffer
};
VkDeviceAddress gpuBufferAddress = vkGetBufferDeviceAddress(device, &addressInfo);

// 3. Push 64-bit Address directly to Shader via Push Constants
struct PushConstantBlock {
    VkDeviceAddress vertexBufferAddress;
    VkDeviceAddress materialBufferAddress;
    glm::mat4 modelMatrix;
};

PushConstantBlock pc{
    .vertexBufferAddress = gpuBufferAddress,
    .materialBufferAddress = gpuMaterialAddress,
    .modelMatrix = glm::mat4(1.0f)
};

vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   0, sizeof(PushConstantBlock), &pc);
```

### GLSL Vertex Shader (`GL_EXT_buffer_reference2`)
```glsl
#version 460
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer VertexBufferRef {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    VertexBufferRef vertexBuffer;
    uint64_t materialBuffer;
    mat4 modelMatrix;
} pc;

layout(location = 0) out vec3 outNormal;

void main() {
    // Direct 64-bit GPU raw pointer dereference
    Vertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = pc.modelMatrix * vec4(v.position, 1.0);
    outNormal = v.normal;
}
```

## Acceptance Criteria
- [x] Enable `bufferDeviceAddress` feature during physical and logical device creation.
- [x] Allocate device memory using `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`.
- [x] Retrieve 64-bit `VkDeviceAddress` for vertex and material structures via `vkGetBufferDeviceAddress`.
- [x] Pass raw 64-bit GPU addresses through `vkCmdPushConstants` without creating `VkDescriptorSet`.
- [x] Compile GLSL shaders with `GL_EXT_buffer_reference2` and verify correct memory dereferencing.
- [x] Render complex 3D meshes using descriptorless buffer pointers with zero validation layer issues.

## Directory Structure
- `src/main.cpp`: BDA zero-copy streaming host application.
- `shaders/bda_mesh.vert`, `shaders/bda_mesh.frag`: Pointer-based GLSL shaders.
- `CMakeLists.txt`: Build target configuration.
