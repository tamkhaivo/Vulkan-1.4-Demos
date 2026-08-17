# Assignment 21 – Bindless Texturing & Non-Uniform Indexing (`GL_EXT_nonuniform_qualifier`)

## Overview
Bind an unbounded array of thousands of textures into a single descriptor table, dynamically indexing individual textures inside fragment shaders using push constants and non-uniform qualifiers.

## Key Concepts
- Descriptor indexing features: `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount`, `shaderSampledImageArrayNonUniformIndexing`.
- Unbounded descriptor array binding (`sampler2D uTextures[]` in GLSL).
- `nonuniformEXT` dynamic texture index qualifiers in shaders.
- Eliminating per-texture descriptor switching and draw call batching overhead.

## Acceptance Criteria
- [x] Enable Vulkan 1.2+ descriptor indexing features in device initialization.
- [x] Create a descriptor set layout with `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT` and variable binding sizes.
- [x] Populate descriptor set with an array of multiple distinct textures without filling unused slots.
- [x] Pass dynamic texture indices to shaders via push constants or vertex attributes.
- [x] Sample textures in GLSL using `texture(uTextures[nonuniformEXT(index)], uv)` correctly.

## Directory Structure
- `src/main.cpp`: Bindless texturing host application.
- `shaders/`: GLSL shaders with `GL_EXT_nonuniform_qualifier` indexing.
- `CMakeLists.txt`: Build target configuration.
