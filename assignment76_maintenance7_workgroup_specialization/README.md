# Assignment 76 – Maintenance 7 Dynamic Workgroup Specialization & Execution Optimization (VK_KHR_maintenance7)

## Overview & Architectural Critique
In Vulkan compute kernels, tuning workgroup dimensions (`local_size_x, y, z`) to match the underlying GPU SM/WGP architecture is critical for achieving peak occupancy. 

**Assignment 76** implements **`VK_KHR_maintenance7`**:
1. **Dynamic Specialization Constants**: Computes hardware wave-size properties and dynamically specializes compute workgroup sizes (`local_size_x_id`) at runtime without invalidating PSO caches.
2. **Maintenance 7 Queries**: Introspects device-level allocation granularity and robust buffer bounds.
3. **High-Performance Wave Particle System**: Executes dynamically sized compute workgroups simulating turbulent physics particles.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_maintenance7`**: Runtime device properties and workgroup execution layout guarantees.
- **Compute Specialization Constants**: `VkSpecializationInfo` mapping hardware subgroup sizing directly into SPIR-V.
- **Compute-to-Graphics Synchronization2 Barrier**: Coherent buffer pipeline transition.

## Acceptance Criteria
- [x] Query physical device properties for subgroup sizing and Maintenance 7 support.
- [x] Configure specialization constants dynamically tailoring compute local sizes.
- [x] Execute particle compute simulation and render results via Dynamic Rendering.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Host application with dynamic specialization setup.
- `shaders/particle_sim.comp`, `shaders/particle_draw.vert`, `shaders/particle_draw.frag`: Compute & graphics shaders.
- `CMakeLists.txt`: Build target configuration.
