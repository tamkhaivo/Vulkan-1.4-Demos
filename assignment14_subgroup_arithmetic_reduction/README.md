# Assignment 14 – Subgroup Operations & Wave-Level Math (`VK_KHR_shader_subgroup_arithmetic`)

## Overview
Perform high-performance, bank-conflict-free parallel compute reductions and SIMD-lane communication using hardware subgroup arithmetic operations.

## Key Concepts
- `VkPhysicalDeviceSubgroupProperties` and subgroup feature enablement (`VK_SUBGROUP_FEATURE_ARITHMETIC_BIT`).
- GLSL `GL_KHR_shader_subgroup_arithmetic` and `GL_KHR_shader_subgroup_basic` extensions.
- Wave-level intrinsics: `subgroupAdd`, `subgroupInclusiveAdd`, `subgroupExclusiveAdd`, `subgroupElect`, and `subgroupBallot`.
- Eliminating workgroup shared memory atomic bottlenecks by performing intra-warp/wavefront SIMD reductions.

## Acceptance Criteria
- [x] Query physical device subgroup size (`subgroupSize`) and supported subgroup operations.
- [x] Write compute shader utilizing `subgroupAdd()` and `subgroupElect()` for parallel data reduction.
- [x] Bind compute storage buffer (SSBO) containing numerical input and output arrays.
- [x] Dispatch compute workgroups and verify correct mathematical reduction across subgroups.
- [x] Render or report reduced results cleanly.

## Directory Structure
- `src/main.cpp`: Subgroup arithmetic compute host application.
- `shaders/`: Compute shader (`subgroup_reduce.comp`).
- `CMakeLists.txt`: Build target configuration.
