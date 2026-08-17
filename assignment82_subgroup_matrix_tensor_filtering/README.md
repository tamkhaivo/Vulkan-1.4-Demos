# Assignment 82 – Sub-Group Matrix Tensor Convolutions & Direct Neural Filtering

## Overview & Architectural Critique
Post-processing passes such as neural super-resolution and path-tracing denoising are bound by scalar compute limits. Leveraging hardware Matrix/Tensor cores directly in compute pipelines enables dense FP16 matrix-multiply-accumulate (MMA) operations with over $5\times$ ALU throughput. **Assignment 82** implements cooperative matrix neural filtering across subgroup wave invocations.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_cooperative_matrix`**: Introspecting matrix sizes ($M, N, K$) and types.
- **Wave-Level Cooperative Instructions**: `coopMatMulAdd` subgroup tiling.
- **Dynamic Rendering**: Visualizing split-screen comparison of noisy vs. tensor-denoised surfaces.

## Acceptance Criteria
- [x] Configure cooperative matrix extensions and pipeline features.
- [x] Render split-screen comparison of high-frequency noisy input vs neural-filtered output.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Neural tensor filtering host pipeline.
- `shaders/tensor_filter.vert`, `shaders/tensor_filter.frag`: Neural filter shaders.
- `CMakeLists.txt`: Build target configuration.
