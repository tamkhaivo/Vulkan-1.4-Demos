# Vulkan 1.4 Learning Specification - Workspace

This repository contains the structured learning path for mastering the Vulkan 1.4 graphics and compute API based on the **Vulkan 1.4 Learning Specification**.

https://docs.vulkan.org/tutorial/latest/00_Introduction.html

## Repository Directory Layout

- `common/`: Shared helper headers, Vulkan initializers, math utilities, and application frameworks.
- `assignment01_hello_triangle/`: **Assignment 1 – Hello Triangle (Dynamic Rendering)**
  - Demonstrates `VkRenderingInfo` dynamic rendering without `VkRenderPass` or `VkFramebuffer` objects.
- `assignment02_rotating_cube/`: **Assignment 2 – Rotating Cube with Uniform Buffers**
  - MVP matrix updates via staging & uniform buffers, descriptor set layouts, and binding contracts.
- `assignment03_textured_quad/`: **Assignment 3 – Textured Quad with Sampler**
  - Image creation, layout transitions (`VkImageMemoryBarrier`), staging uploads, and combined image samplers.
- `assignment04_push_constants_dynamic_uniforms/`: **Assignment 4 – Push Constants and Dynamic Uniform Buffers**
  - Per-object matrix push constants (`VkPushConstantRange`) & dynamic buffer offsets (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`).
- `assignment05_instanced_rendering/`: **Assignment 5 – Instanced Rendering with Vertex Attribute Divisor**
  - Mass instancing using `VkPipelineVertexInputDivisorStateCreateInfoKHR` / per-instance divisors.
- `assignment06_two_pass_dynamic_rendering_local_read/`: **Assignment 6 – Two-Pass Effect with Dynamic Rendering Local Reads**
  - On-chip attachment reading via `VK_KHR_dynamic_rendering_local_read` and `VkRenderingInputAttachmentInfoKHR`.
- `assignment07_compute_particles_indirect_draw/`: **Assignment 7 – Compute Particle System with Indirect Draw**
  - Compute dispatches (`vkCmdDispatch`), storage buffers (`SSBO`), buffer barriers, and GPU-driven `vkCmdDrawIndirect`.
- `assignment08_deferred_shading_g_buffer/`: **Assignment 8 – Deferred Shading with Multiple Render Targets (Dynamic Rendering Local Reads)**
  - G-Buffer layout (Albedo, Normal, Depth) with dynamic rendering local reads in a single fragment shader pass.
- `assignment09_multithreaded_command_recording/`: **Assignment 9 – Multi-Threaded Command Recording with Timeline Semaphores**
  - Multi-threaded secondary command buffer recording and timeline semaphore CPU/GPU synchronization (`VkSemaphoreTypeCreateInfo`).
- `assignment10_buffer_device_address_streaming/`: **Assignment 10 – Buffer Device Address and Zero-Copy Streaming**
  - Bindless shader access via `VK_KHR_buffer_device_address` (`GL_EXT_buffer_reference`), persistent mapping, and ReBAR streaming.
- `assignment11_mesh_task_shading/`: **Assignment 11 – Modern Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader`)**
  - Task and mesh shaders replacing fixed-function input assembly, dynamic meshlet generation, and `vkCmdDrawMeshTasksEXT`.
- `assignment12_descriptor_buffers/`: **Assignment 12 – Modern Bindless with Descriptor Buffers (`VK_EXT_descriptor_buffer`)**
  - Pool-less descriptor memory packing, direct buffer binding (`vkCmdBindDescriptorBuffersEXT`), and push descriptors.
- `assignment13_pipeline_binaries_cache/`: **Assignment 13 – Pipeline Binaries & Cache Optimization (`VK_KHR_pipeline_binary` / `VkPipelineCache`)**
  - Hitch-free PSO pre-warming, disk cache serialization, and Vulkan 1.4 pipeline binary querying.
- `assignment14_subgroup_arithmetic_reduction/`: **Assignment 14 – Subgroup Operations & Wave-Level Math (`VK_KHR_shader_subgroup_arithmetic`)**
  - Cross-SIMD lane arithmetic reduction (`subgroupAdd`, `subgroupElect`), ballot voting, and bank-conflict-free parallel compute.
- `assignment15_ray_queries_inline/`: **Assignment 15 – Hardware Ray Queries & Inline Traversal (`VK_KHR_ray_query`)**
  - Ray queries in compute shaders (`rayQueryInitializeEXT`, `rayQueryProceedEXT`), BLAS/TLAS acceleration structures, and real-time shadow queries.

## Build Instructions (Clang 17+ & Vulkan 1.4 Standard)

### 1. Configure with Clang Toolset & Vulkan 1.4 SDK:
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -T ClangCL -DCMAKE_PREFIX_PATH="C:/Users/tamkh/Documents/Type0/build/vcpkg_installed/x64-windows"
```

### 2. Build Target (e.g. Assignment 1):
```powershell
cmake --build build --target assignment01_hello_triangle --config Debug
```

### 3. Run Executable:
```powershell
.\build\bin\Debug\assignment01_hello_triangle.exe
```

