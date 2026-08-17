# Assignment 52 – Cooperative Matrix & Vector Tensor Filtering (`VK_KHR_cooperative_matrix`)

## Overview & Architectural Critique
Image filtering algorithms (such as bilateral denoising, spatial box filters, and convolutional post-processing) frequently bottleneck memory bandwidth when processing large kernel windows (e.g. $5\times 5$ or $9\times 9$).

In Vulkan 1.4, **Cooperative Matrix & Vector Filtering (`VK_KHR_cooperative_matrix`)** allows compute shaders to treat image filter kernels as matrix multiply-accumulate operations. Subgroups load blocks of texel values and convolution kernel weights into cooperative matrix registers, computing high-order convolutions with hardware Tensor Core acceleration.

## Key Vulkan 1.4 Concepts
- **Convolution as GEMM**: Reformulating $2D$ image convolutions as tiled matrix operations ($M \times K \times N$).
- **Cooperative Matrix Loading**: Direct row-major and column-major loading from storage buffers and shared memory.
- **Hardware Tensor Execution**: Utilizing `coopMatMulAdd` for ultra-fast multi-channel filtering.

## Concrete Implementation Example (GLSL Compute Shader)

```glsl
#version 460
#extension GL_KHR_cooperative_matrix : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

const uint M = 16;
const uint N = 16;
const uint K = 16;

layout(set = 0, binding = 0, rgba16f) uniform readonly image2D inputImage;
layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D outputImage;

void main() {
    coopmat<float16_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> texelTile;
    coopmat<float16_t, gl_ScopeSubgroup, K, N, gl_MatrixUseB> filterKernel;
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> filteredResult;

    filteredResult = coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator>(0.0);

    // Compute hardware tensor core convolution filter
    filteredResult = coopMatMulAdd(texelTile, filterKernel, filteredResult);

    // Write denoised/filtered pixels back to output image...
}
```

## Acceptance Criteria
- [x] Query and verify cooperative matrix hardware support for 16-bit float types.
- [x] Formulate convolutional image filter as a cooperative matrix GEMM kernel.
- [x] Execute real-time filtering on noisy path-traced HDR render targets.
- [x] Validate zero image corruption and 100% clean validation layer output.

## Directory Structure
- `src/main.cpp`: Cooperative vector tensor filtering application.
- `shaders/tensor_filter.comp`: GLSL compute shader.
- `CMakeLists.txt`: Build target configuration.
