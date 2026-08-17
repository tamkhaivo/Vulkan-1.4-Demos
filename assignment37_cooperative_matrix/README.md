# Assignment 37 – Cooperative Matrix & Neural Denoising / Super-Resolution (`VK_KHR_cooperative_matrix`)

## Overview
Leverage GPU Tensor/Matrix Hardware Cores directly within Vulkan compute shaders using `VK_KHR_cooperative_matrix` to accelerate matrix multiply-accumulate ($C = A \times B + C$) operations for real-time neural path tracing denoising or latent physics inference.

## Key Concepts
- `VK_KHR_cooperative_matrix` / `GL_KHR_cooperative_matrix` SPIR-V and GLSL enablement.
- Querying supported cooperative matrix properties (`vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR`).
- Cooperative matrix types in GLSL: `coopmat<float16_t, gl_ScopeSubgroup, M, N, MatrixUseA>`.
- Matrix tile cooperative loading (`coopMatLoad`), multiply-accumulate (`coopMatMulAdd`), and cooperative storing (`coopMatStore`).
- Subgroup SIMD data distribution and cache tiling for high-throughput GEMM kernels.

## Acceptance Criteria
- [x] Enumerate and select valid matrix dimensions ($M, N, K$) and data types (`Float16` / `Int8` / `Float32`) from physical device properties.
- [x] Implement a compute shader executing tiled GEMM using `coopmat` types and `coopMatMulAdd`.
- [x] Synchronize shared memory tiles and subgroup lanes for neural filtering of noisy input buffers.
- [x] Verify arithmetic accuracy and measure teraflops / execution speedup compared to standard compute shaders.
- [x] Output a real-time denoised image or matrix transformation visualization.

## Directory Structure
- `src/main.cpp`: Cooperative matrix device query, pipeline dispatch, and neural tensor evaluation.
- `CMakeLists.txt`: Build target configuration.
