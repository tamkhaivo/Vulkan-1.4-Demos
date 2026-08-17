# Assignment 71 – Nanite-Style Micro-Polygon Software Rasterizer via 64-bit Atomics (VK_EXT_shader_atomic_float2 / 64-bit Atomics)

## Overview & Architectural Critique
Hardware rasterizers on modern GPUs operate on fixed $2 \times 2$ pixel quads. When geometry is micro-polygon dense (triangles smaller than $4\text{px}^2$ or sub-pixel sized), up to 75% to 90% of shader invocations inside helper pixels are discarded, incurring massive overdraw and quad inefficiency bottlenecks.

**Assignment 71** implements a **Nanite-Style Software Rasterizer**:
1. **GPU Compute-Driven Micro-Rasterization**: Executes compute workgroups testing sub-pixel micro-triangles directly against screen pixels using 2D edge equations.
2. **64-bit Atomic Visibility Buffer**: Uses Vulkan 1.4 64-bit integer atomics (`atomicMin` / `VK_KHR_shader_atomic_int64` / `VK_EXT_shader_atomic_float2`) to perform hardware-free depth testing and cluster/primitive ID encoding in a single atomic instruction: `uint64_t(depth32 << 32 | cluster_tri_id)`.
3. **Screen-Space Visibility Resolve Pass**: A subsequent fullscreen dynamic rendering pass reads the visibility buffer, extracts the winning triangle, and performs material evaluation with zero sub-pixel quad overdraw waste.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_shader_atomic_int64` / 64-Bit Device Atomics**: Atomic 64-bit payload writes in GPU device memory without mutexes.
- **Compute Rasterizer Core**: SIMD workgroups evaluating triangle bounding boxes and Barycentric edge equations.
- **Dynamic Rendering & Synchronization2**: Coherent compute-to-fragment synchronization (`VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT` to `VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT`).

## Concrete Visual Example (Compute Rasterizer Kernel)

```glsl
#version 460
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer VisBuffer {
    uint64_t visibilityBuffer[]; // Packed Depth (MSB 32-bit) + TriID (LSB 32-bit)
};

struct MicroTriangle {
    vec4 v0, v1, v2;
    uint triId;
};

// Rasterize micro-triangle directly into 64-bit atomic visibility buffer
void rasterizePixel(ivec2 p, MicroTriangle tri, ivec2 screenSize) {
    if (p.x < 0 || p.x >= screenSize.x || p.y < 0 || p.y >= screenSize.y) return;
    
    // Evaluate 2D edge equations...
    float depth = computeInterpolatedDepth(p, tri);
    uint depthUint = floatBitsToUint(depth);
    uint64_t packedVal = (uint64_t(depthUint) << 32) | uint64_t(tri.triId);
    
    uint pixelIndex = p.y * screenSize.x + p.x;
    atomicMin(visibilityBuffer[pixelIndex], packedVal);
}
```

## Acceptance Criteria
- [x] Configure physical device features for 64-bit integer atomics (`shaderBufferInt64Atomics`).
- [x] Implement compute shader software rasterization kernel testing micro-polygon clusters.
- [x] Execute fullscreen dynamic rendering pass resolving the 64-bit packed visibility buffer.
- [x] Render animated dense micro-geometry with clean depth sorting and 0 validation errors.

## Directory Structure
- `src/main.cpp`: Software rasterizer application host and Vulkan 1.4 pipeline manager.
- `shaders/soft_raster.comp`: Micro-triangle compute rasterization kernel.
- `shaders/vis_resolve.vert`, `shaders/vis_resolve.frag`: Screen-space visibility buffer resolve pipeline.
- `CMakeLists.txt`: Build target configuration.
