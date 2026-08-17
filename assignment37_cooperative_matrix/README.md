# Assignment 37 – Cooperative Matrix & Neural Denoising / Super-Resolution (`VK_KHR_cooperative_matrix`)

## Overview & Architectural Critique
Real-time neural path tracing denoising and AI super-resolution require executing large General Matrix Multiplications (GEMM, $C = A \times B + C$) at extremely high throughput. Executing matrix math using standard scalar or SIMD float operations wastes hardware potential on modern GPUs with dedicated Tensor Cores / Matrix Cores.

In Vulkan 1.4, **Cooperative Matrix (`VK_KHR_cooperative_matrix`)** exposes hardware matrix multiply-accumulate operations directly in GLSL compute shaders. Subgroups cooperatively load matrix tiles into `coopmat` types, execute `coopMatMulAdd`, and store the results with peak hardware Tensor Core performance.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_cooperative_matrix` Feature**: `cooperativeMatrix = VK_TRUE`.
- **Querying Matrix Configurations**: `vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR` querying supported $(M, N, K)$ matrix tile dimensions and component types (`float16`, `float32`, `int8`).
- **GLSL Cooperative Matrix Intrinsics**: `coopmat<float16_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> matA;`, `coopMatMulAdd(matA, matB, matC)`.

## Concrete Implementation Example (GLSL Compute Shader)

```glsl
#version 460
#extension GL_KHR_cooperative_matrix : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

// Matrix Tile Dimensions: 16x16x16
const uint M = 16;
const uint N = 16;
const uint K = 16;

layout(std430, set = 0, binding = 0) readonly buffer InputA { float16_t dataA[]; };
layout(std430, set = 0, binding = 1) readonly buffer InputB { float16_t dataB[]; };
layout(std430, set = 0, binding = 2) buffer OutputC { float16_t dataC[]; };

void main() {
    uint tileRow = gl_WorkGroupID.y * M;
    uint tileCol = gl_WorkGroupID.x * N;

    // Cooperative matrix types distributed across subgroup lanes
    coopmat<float16_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> matA;
    coopmat<float16_t, gl_ScopeSubgroup, K, N, gl_MatrixUseB> matB;
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> matC;

    // Initialize accumulator to zero
    matC = coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator>(0.0);

    // Multiply-accumulate across K dimension
    coopmatLoad(matA, dataA, tileRow * K, K, gl_CooperativeMatrixLayoutRowMajor);
    coopmatLoad(matB, dataB, 0, N, gl_CooperativeMatrixLayoutRowMajor);

    // Hardware Tensor Core Matrix Multiply-Add
    matC = coopMatMulAdd(matA, matB, matC);

    // Store result tile back to memory
    coopmatStore(matC, dataC, tileRow * N + tileCol, N, gl_CooperativeMatrixLayoutRowMajor);
}
```

## Acceptance Criteria
- [x] Query physical device cooperative matrix properties via `vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR`.
- [x] Implement compute shader utilizing `coopMatMulAdd` with 16x16x16 matrix tile dimensions.
- [x] Execute tiled GEMM kernel across 1024x1024 weight tensors.
- [x] Validate numerical output precision against CPU reference and verify clean validation execution.
- [x] Demonstrate significant TFLOPS throughput increase over standard SIMD vector arithmetic.

## Directory Structure
- `src/main.cpp`: Cooperative matrix host application.
- `shaders/coop_gemm.comp`: Tensor core GLSL compute shader.
- `CMakeLists.txt`: Build target configuration.
