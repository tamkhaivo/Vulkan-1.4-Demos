# Assignment 52 – Cooperative Vector & Subgroup Matrix Convolution (`VK_NV_cooperative_vector` / `VK_KHR_cooperative_matrix`)

## Overview
Implement high-throughput real-time image filtering and post-processing (e.g. bilateral denoising, spatial convolution, latent tensor multiplication) utilizing Tensor Core matrix instructions operating across SIMD subgroup lanes.

## Key Concepts
- `VK_KHR_cooperative_matrix` / `VK_NV_cooperative_vector` features.
- Loading image tiles directly into hardware matrix registers (`coopmat::coopMatLoadNV`).
- Subgroup matrix-vector and matrix-matrix multiply-accumulate (`coopMatMulAdd`).
- Precision modes (`float16_t`, `bfloat16_t`, `int8_t`) with subgroup cooperative execution.

## Acceptance Criteria
- [x] Query cooperative matrix properties ($M \times N \times K$ configurations and scopes).
- [x] Write compute kernel mapping image tiles into cooperative matrix structures.
- [x] Execute hardware-accelerated matrix-matrix multiply-accumulate across subgroup lanes.
- [x] Generate filtered HDR buffer and verify numeric accuracy.
- [x] Demonstrate significant speedup over standard scalar compute loops.

## Directory Structure
- `src/main.cpp`: Cooperative matrix setup and convolution dispatch host application.
- `CMakeLists.txt`: Build target configuration.
