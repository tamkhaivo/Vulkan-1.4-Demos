# Assignment 88 – Dynamic Multi-Queue Async Physics & Compute-to-Direct-Meshlet Stream

## Overview & Architectural Critique
Decoupling physics simulations from graphical frame rates prevents physics stutter during rendering spikes. **Assignment 88** executes a high-density wave/cloth simulation on an asynchronous compute pipeline that streams updated vertex attributes directly into dynamic rendering shaders via timeline semaphores and buffer ownership handoffs.

## Key Vulkan 1.4 Concepts
- **Multi-Queue Concurrency**: Dedicated compute and graphics coordination.
- **Timeline Semaphores**: Monotonic progress counters ($120\text{Hz}$ compute $\to 60\text{Hz}$ render).
- **Dynamic Rendering**: Rendering animated dynamic cloth/water meshlets.

## Acceptance Criteria
- [x] Configure dedicated multi-queue and timeline synchronization structures.
- [x] Stream dynamic compute physics buffers directly into rasterization.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Async physics to meshlet stream pipeline.
- `shaders/physics_meshlet.vert`, `shaders/physics_meshlet.frag`: Meshlet shaders.
- `CMakeLists.txt`: Build target configuration.
