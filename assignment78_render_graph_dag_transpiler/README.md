# Assignment 78 – Multi-Queue Timeline Render Graph with Automatic Synchronization2 Transpiler (Render Graph DAG / Synchronization2 Auto-Hazards)

## Overview & Architectural Critique
In complex multi-pass rendering engines with asynchronous Compute, Graphics, and DMA Transfer queues, manual barrier placement leads to race hazards, pipeline serialization, or validation errors.

**Assignment 78** implements a **Production Render Graph DAG Compiler**:
1. **DAG Topological Pass Scheduler**: Passes declare inputs/outputs; the graph compiler automatically discovers dependencies and sorts passes.
2. **Hazard Tracking Engine**: Automatically resolves RAW (Read-After-Write), WAR, and WAW hazards, synthesizing minimal `VkPipelineBarrier2` and `VkDependencyInfo` structs.
3. **Multi-Pass Visual Pipeline**: Compiles and executes Depth Pre-pass, Main Lit Shading, Post-Processing Bloom Blur, and Tone Mapping passes with zero manual synchronization code.

## Key Vulkan 1.4 Concepts
- **Automated Synchronization2**: Minimal stage masks and access masks generated via graph hazard detection.
- **DAG Pass Scheduling**: Topological sort and resource lifetime aliasing.
- **Dynamic Rendering**: Multi-pass rendering orchestrated entirely without `VkRenderPass`.

## Acceptance Criteria
- [x] Implement DAG Render Graph compiler with resource hazard detection.
- [x] Auto-generate Vulkan 1.4 `VkDependencyInfo` barriers.
- [x] Execute multi-pass rendering pipeline (Scene Pass -> Post Pass -> Composite Pass).
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Host application with DAG compiler and multi-pass pipeline.
- `shaders/scene.vert`, `shaders/scene.frag`, `shaders/post_bloom.vert`, `shaders/post_bloom.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
