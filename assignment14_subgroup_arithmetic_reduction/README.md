# Assignment 14 – Subgroup Operations & Wave-Level Math (`VK_KHR_shader_subgroup_arithmetic`)

## Overview & Architectural Critique
Parallel reductions across GPU compute workgroups traditionally rely on workgroup shared memory (`shared float data[]`) with multiple barrier synchronizations (`barrier()`), leading to bank conflicts and execution bubbles.

In Vulkan 1.4, **Subgroup Operations (`VK_KHR_shader_subgroup_arithmetic` / Vulkan 1.4 Core Subgroups)** enable direct register-to-register communication between SIMD lanes (subgroups/waves) without using shared memory. By using `subgroupAdd`, `subgroupMin`, `subgroupMax`, and `subgroupElect`, compute shaders achieve optimal throughput for prefix sums, histograms, and parallel reductions.

## Key Vulkan 1.4 Concepts
- **Vulkan 1.4 Core Subgroups**: `subgroupSize`, `supportedStages`, and `supportedOperations` containing `VK_SUBGROUP_FEATURE_ARITHMETIC_BIT` and `VK_SUBGROUP_FEATURE_BALLOT_BIT`.
- **Wave-Level Reductions**: `subgroupAdd(value)`, `subgroupInclusiveAdd(value)`, `subgroupExclusiveAdd(value)`.
- **Leader Election**: `if (subgroupElect())` to execute atomic workgroup operations once per warp/wave instead of per thread.

## Concrete Implementation Example (GLSL Compute Shader)

```glsl
#version 460
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) readonly buffer InputBuffer {
    float values[];
};

layout(std430, set = 0, binding = 1) buffer OutputBuffer {
    float totalSum;
};

shared float sharedSubgroupSums[8]; // For 256 threads with warp size 32 (8 subgroups)

void main() {
    uint globalId = gl_GlobalInvocationID.x;
    float val = values[globalId];

    // 1. Cross-SIMD lane arithmetic reduction within subgroup (No shared memory, zero barrier)
    float subgroupSum = subgroupAdd(val);

    // 2. Elect one leader per subgroup to write the subgroup reduction
    if (subgroupElect()) {
        sharedSubgroupSums[gl_SubgroupID] = subgroupSum;
    }

    barrier(); // Synchronize only once across the 8 subgroup results

    // 3. First subgroup performs the final reduction of the partial sums
    if (gl_SubgroupID == 0) {
        float finalPartial = (gl_SubgroupInvocationID < gl_NumSubgroups) ? sharedSubgroupSums[gl_SubgroupInvocationID] : 0.0;
        float workgroupTotal = subgroupAdd(finalPartial);
        
        if (subgroupElect()) {
            atomicAdd(totalSum, workgroupTotal);
        }
    }
}
```

## Acceptance Criteria
- [x] Query physical device subgroup properties (`VkPhysicalDeviceSubgroupProperties`) to verify arithmetic subgroup support.
- [x] Implement compute reduction shader using `subgroupAdd` and `subgroupElect`.
- [x] Dispatch compute kernel over 1,000,000+ elements in an SSBO.
- [x] Verify mathematical correctness of the parallel reduction on CPU host readback.
- [x] Demonstrate significant reduction in workgroup barrier stalls compared to traditional shared-memory reductions.

## Directory Structure
- `src/main.cpp`: Subgroup arithmetic reduction application.
- `shaders/subgroup_reduce.comp`: Wave-level GLSL compute shader.
- `CMakeLists.txt`: Build target configuration.
