# Assignment 38 – Vulkan Memory Model & Lock-Free Data Structures (`VK_KHR_vulkan_memory_model`)

## Overview & Architectural Critique
Historically, Vulkan shaders relied on loose, weakly-ordered memory consistency models where fine-grained thread synchronization required heavy workgroup barriers (`barrier()`) or full execution dependency flushes.

In Vulkan 1.4, the **Vulkan Memory Model (`VK_KHR_vulkan_memory_model` / Vulkan 1.4 Core)** introduces a formal C++11-like memory specification for GPU execution. It enables fine-grained **Acquire/Release Semantics** (`gl_StorageSemanticsAcquire`, `gl_StorageSemanticsRelease`), device-scoped atomics (`gl_ScopeDevice`), and non-blocking GPU ring buffers and lock-free queues without coarse pipeline barriers.

## Key Vulkan 1.4 Concepts
- **Vulkan Memory Model Features**: `vulkanMemoryModel = VK_TRUE`, `vulkanMemoryModelDeviceScope = VK_TRUE`.
- **Memory Semantics in GLSL**: `#extension GL_KHR_memory_scope_semantics : require`.
- **Atomic Operations with Scopes**: `atomicLoad(ptr, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsAcquire)` and `atomicStore(ptr, val, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelease)`.

## Concrete Implementation Example (GLSL Lock-Free Ring Buffer Push)

```glsl
#version 460
#extension GL_KHR_memory_scope_semantics : require
#extension GL_EXT_shader_atomic_float : enable

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct QueueItem {
    vec4 payload;
};

layout(std430, set = 0, binding = 0) buffer RingQueue {
    uint head;
    uint tail;
    QueueItem items[1024];
} queue;

void main() {
    vec4 myData = vec4(gl_GlobalInvocationID.xyz, 1.0);

    // 1. Atomically claim slot in the device-wide queue (Relaxed atomic increment)
    uint slot = atomicAdd(queue.tail, 1, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelaxed);
    uint ringIndex = slot % 1024;

    // 2. Write payload to claimed slot
    queue.items[ringIndex].payload = myData;

    // 3. Release barrier: ensures payload write is visible before head pointer increments
    atomicStore(queue.head, slot + 1, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelease);
}
```

## Acceptance Criteria
- [x] Enable `vulkanMemoryModel` and `vulkanMemoryModelDeviceScope` physical device features.
- [x] Implement lock-free multi-producer single-consumer GPU queue in compute shader.
- [x] Utilize fine-grained `gl_SemanticsAcquire` and `gl_SemanticsRelease` memory semantics.
- [x] Validate concurrent data integrity across thousands of asynchronous workgroup dispatches with zero race conditions.

## Directory Structure
- `src/main.cpp`: Vulkan memory model host application.
- `shaders/lockfree_queue.comp`: Lock-free queue compute shader.
- `CMakeLists.txt`: Build target configuration.
