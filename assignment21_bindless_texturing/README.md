# Assignment 21 – Bindless Texturing & Non-Uniform Indexing (`GL_EXT_nonuniform_qualifier`)

## Overview & Architectural Critique
In complex 3D scenes with hundreds or thousands of distinct materials, switching descriptor sets between draw calls creates severe CPU driver overhead and prevents effective draw call batching.

In Vulkan 1.4, **Bindless Texturing** enables binding thousands of textures into a single unbounded or massive descriptor array (`sampler2D uTextures[]`). Shaders dynamically index into this array using push constants or per-instance attributes via `nonuniformEXT()`, ensuring warp lanes can sample different textures divergence-free.

## Key Vulkan 1.4 Concepts
- **Descriptor Indexing Features**: `descriptorBindingPartiallyBound = VK_TRUE`, `descriptorBindingVariableDescriptorCount = VK_TRUE`, `runtimeDescriptorArray = VK_TRUE`, `shaderSampledImageArrayNonUniformIndexing = VK_TRUE`.
- **Descriptor Binding Flags**: `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT`.
- **GLSL Non-Uniform Indexing**: `#extension GL_EXT_nonuniform_qualifier : require` and `texture(uTextures[nonuniformEXT(materialIndex)], uv)`.

## Concrete Implementation Example (Vulkan 1.4 C++ & GLSL)

### C++ Host Setup
```cpp
// 1. Configure Descriptor Binding Flags for Bindless Array
VkDescriptorBindingFlags bindingFlags = 
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = 1,
    .pBindingFlags = &bindingFlags
};

VkDescriptorSetLayoutBinding bindlessBinding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    .descriptorCount = 4096, // Bindless array of 4,096 textures
    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    .pImmutableSamplers = nullptr
};

VkDescriptorSetLayoutCreateInfo layoutInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = &flagsInfo,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .bindingCount = 1,
    .pBindings = &bindlessBinding
};
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &bindlessLayout);
```

### GLSL Fragment Shader
```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inMaterialID;

layout(set = 0, binding = 0) uniform sampler2D uTextures[];

layout(location = 0) out vec4 outColor;

void main() {
    // Non-uniform safe dynamic indexing into bindless texture table
    outColor = texture(uTextures[nonuniformEXT(inMaterialID)], inUV);
}
```

## Acceptance Criteria
- [x] Enable Vulkan 1.4 descriptor indexing features (`descriptorBindingPartiallyBound`, `shaderSampledImageArrayNonUniformIndexing`).
- [x] Create an unbounded descriptor set layout with `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT`.
- [x] Populate descriptor table with 500+ textures of varying formats and dimensions.
- [x] Render multi-material geometry dynamically indexing textures via push constants / flat vertex attributes.
- [x] Verify zero descriptor rebinding overhead across draw calls and clean validation output.

## Directory Structure
- `src/main.cpp`: Bindless texturing host application.
- `shaders/bindless.vert`, `shaders/bindless.frag`: Non-uniform texture indexing shaders.
- `CMakeLists.txt`: Build target configuration.
