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
- `assignment16_gpu_driven_draw_indirect_count/`: **Assignment 16 – GPU-Driven Scene Culling & Multi-Draw Indirect Count (`VK_KHR_draw_indirect_count`)**
  - GPU compute-driven frustum & occlusion culling, dynamic draw command generation in SSBOs, and `vkCmdDrawIndexedIndirectCountKHR`.
- `assignment17_shader_objects/`: **Assignment 17 – Next-Gen Pipeline Flexibility with Shader Objects (`VK_EXT_shader_object`)**
  - Decoupled shader stage creation (`vkCreateShadersEXT`), dynamic state binding (`vkCmdBindShadersEXT`), and pipeline-free architectures.
- `assignment18_hardware_ray_tracing_pipeline/`: **Assignment 18 – Full Hardware Ray Tracing Pipeline & Shader Binding Tables (`VK_KHR_ray_tracing_pipeline`)**
  - RayGen, Closest-Hit, Miss shaders, Shader Binding Tables (SBT) stride management, and `vkCmdTraceRaysKHR`.
- `assignment19_variable_rate_shading/`: **Assignment 19 – Hardware Variable Rate Shading & Density Maps (`VK_KHR_fragment_shading_rate`)**
  - Dynamic fragment shading rates (`1x1`, `2x2`, `4x4`), shading rate combiner operations, and attachment-driven density maps.
- `assignment20_sparse_virtual_texturing/`: **Assignment 20 – Sparse Virtual Texturing & Residency Streaming (`sparseResidencyImage2D`)**
  - Virtual texturing page tables, `vkQueueBindSparse` page allocation, residency mip-tail packing, and minimal VRAM footprints.
- `assignment21_bindless_texturing/`: **Assignment 21 – Bindless Texturing & Non-Uniform Indexing (`GL_EXT_nonuniform_qualifier`)**
  - Massive unbounded sampler arrays (`sampler2D uTextures[]`), `descriptorBindingPartiallyBound`, and dynamic push constant texture dispatch.
- `assignment22_async_compute_transfer_overlap/`: **Assignment 22 – Asynchronous Multi-Queue Concurrency & Transfer Overlap**
  - Concurrent execution across dedicated Graphics, Async Compute, and DMA Transfer queues with timeline semaphores and buffer ownership transfers.
- `assignment23_conditional_rendering_occlusion_queries/`: **Assignment 23 – Hardware Occlusion Queries & Conditional Rendering (`VK_EXT_conditional_rendering`)**
  - Zero-CPU-latency GPU draw command skipping with hardware query pools and conditional predication buffers.
- `assignment24_dynamic_rendering_msaa_resolve/`: **Assignment 24 – Direct Dynamic Rendering Multisampled Resolves & MSAA**
  - Hardware 4x/8x MSAA with inline `VkRenderingAttachmentInfo.resolveMode` color/depth resolves directly on GPU tile memory.
- `assignment25_clustered_forward_lighting/`: **Assignment 25 – Clustered Forward 3D Tile Lighting & Workgroup Compute**
  - 3D view-frustum AABB clustering, workgroup shared memory light culling, and forward evaluation of 1,024 dynamic point lights.
- `assignment26_device_generated_commands/`: **Assignment 26 – Device Generated Commands (`VK_NV_device_generated_commands` / `VK_EXT_device_generated_commands`)**
  - GPU-side command generation, token streams, dynamic PSO switching, and direct device draw call preprocessing.
- `assignment27_extended_dynamic_state3/`: **Assignment 27 – Extended Dynamic State 3 & Vulkan 1.4 Dynamic Pipelines (`VK_EXT_extended_dynamic_state3`)**
  - Dynamic polygon mode, dynamic rasterization samples, and blend equations eliminating monolithic PSO bloat.
- `assignment28_calibrated_timestamps_gpu_profiling/`: **Assignment 28 – Calibrated Timestamps & Hardware Clock Profiling (`VK_KHR_calibrated_timestamps`)**
  - Correlating GPU clock domains with CPU monotonic timers for nanosecond-accurate GPU profiling and frame pacing.
- `assignment29_host_image_copy/`: **Assignment 29 – Host Image Copy & Direct Host Uploads (`VK_EXT_host_image_copy`)**
  - Direct CPU memory-to-image uploads bypassing staging buffer allocations and command buffer submissions.
- `assignment30_mesh_shading_culling_lod/`: **Assignment 30 – Mesh Shading Cluster Culling & LOD Morphing (`VK_EXT_mesh_shader`)**
  - Task/amplification shader backface cone culling, frustum sphere culling, and dynamic meshlet LOD morphing.
- `assignment31_maintenance5_maintenance6/`: **Assignment 31 – Vulkan 1.4 Maintenance 5 & Maintenance 6 (`VK_KHR_maintenance5` / `VK_KHR_maintenance6`)**
  - Shader staging copies, dynamic index range bounds (`vkCmdBindIndexBuffer2KHR`), and non-zero first index configurations.
- `assignment32_opacity_micromaps/`: **Assignment 32 – Hardware Ray Tracing Opacity Micromaps (`VK_EXT_opacity_micromap`)**
  - Hardware micro-opacity arrays built into BLAS to eliminate Any-Hit shader execution stalls and warp divergence for alpha-tested vegetation.
- `assignment33_subgroup_partitioned_quad/`: **Assignment 33 – Subgroup Advanced Partitioning & Quad Operations (`VK_NV_shader_subgroup_partitioned` / Subgroup Quad)**
  - `subgroupPartitionNV()` lock-free GPU binning and quad swaps (`subgroupQuadSwapHorizontal`) for analytic screen-space derivatives.
- `assignment34_dynamic_rendering_suspend_resume/`: **Assignment 34 – Dynamic Rendering Suspend/Resume & Attachment Feedback Loops**
  - Multi-pass render pass continuation across command buffers (`VK_RENDERING_SUSPENDING_BIT` / `VK_RENDERING_RESUMING_BIT`) and programmable feedback blending (`VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT`).
- `assignment35_shader_execution_reordering/`: **Assignment 35 – Shader Execution Reordering (SER) & Position Fetch (`VK_NV_shader_execution_reorder` / `VK_KHR_ray_tracing_position_fetch`)**
  - Path tracing ray divergence mitigation with `hitObjectNV` / `reorderThreadNV()`, and direct vertex extraction from acceleration structure leaves.
- `assignment36_ray_tracing_motion_blur/`: **Assignment 36 – Ray Tracing Motion Blur & Time-Varying BVHs (`VK_NV_ray_tracing_motion_blur`)**
  - Time-varying motion BLAS/TLAS construction, matrix interpolation, and `traceRayMotionNV()` temporal ray sampling.
- `assignment37_cooperative_matrix/`: **Assignment 37 – Cooperative Matrix & Neural Denoising / Super-Resolution (`VK_KHR_cooperative_matrix`)**
  - Tensor Core hardware matrix multiply-accumulate (`coopmat` types, `coopMatMulAdd`) for real-time neural path tracing denoising.
- `assignment38_vulkan_memory_model/`: **Assignment 38 – Vulkan Memory Model & Lock-Free Data Structures (`VK_KHR_vulkan_memory_model`)**
  - Fine-grained Acquire/Release semantics, device-scoped atomics, and lock-free GPU queues eliminating monolithic barrier stalls.
- `assignment39_displacement_micromaps/`: **Assignment 39 – Displacement Micromaps & Micro-Mesh Ray Tracing (`VK_NV_displacement_micromap`)**
  - Sub-triangle micro-displacement embedded directly into BVHs with `VkMicromapNV` for dense geometry with zero Any-Hit cost.
- `assignment40_dynamic_rendering_unused_attachments/`: **Assignment 40 – Dynamic Rendering Unused Attachments & Modular Passes (`VK_EXT_dynamic_rendering_unused_attachments`)**
  - Dynamic attachment subset binding with `VK_ATTACHMENT_UNUSED` and `VK_NULL_HANDLE`, eliminating PSO recompilations.
- `assignment41_multiview_stereo_vr/`: **Assignment 41 – Multi-View Stereo & Foveated VR Rendering (`VK_KHR_multiview` / Vulkan 1.4 Core)**
  - Single-draw broadcast to multiple eye layers via `viewMask` and `gl_ViewIndex` for 50% CPU VR recording reduction.
- `assignment42_custom_border_color_sampler/`: **Assignment 42 – Custom Border Colors & Advanced Sampler Swizzling (`VK_EXT_custom_border_color`)**
  - Arbitrary RGBA border clear values with `VkSamplerCustomBorderColorCreateInfoEXT` to prevent shadow and atlas edge bleeding.
- `assignment43_cluster_acceleration_structure/`: **Assignment 43 – Ray Tracing Partitioned Clusters & BVH Compaction (`VK_NV_cluster_acceleration_structure`)**
  - Cluster-level acceleration structures (CLAS) underneath BLAS for microsecond GPU dynamic LOD rebuilds.
- `assignment44_external_memory_interop/`: **Assignment 44 – External Memory Interop & CUDA/Direct3D 12 Synchronization (`VK_KHR_external_memory_win32`)**
  - Zero-copy resource sharing across Vulkan, CUDA, and Direct3D 12 using Windows NT handles and shared timeline semaphores.
- `assignment45_pipeline_robustness_fault_tolerance/`: **Assignment 45 – Robustness2, Pipeline Robustness & Fault Tolerance (`VK_EXT_pipeline_robustness`)**
  - Per-pipeline out-of-bounds safety, null descriptor bindings, and deterministic zero-return error mitigation without driver penalty.
- `assignment46_push_descriptors/`: **Assignment 46 – Zero-Allocation Push Descriptors (`VK_KHR_push_descriptor` / Vulkan 1.4 Core)**
  - Direct recording of uniform and storage descriptors into command buffers bypassing descriptor pool contention.
- `assignment47_multiview_mesh_shading/`: **Assignment 47 – Multi-View Mesh & Task Shading Pipeline (`VK_EXT_mesh_shader` + `VK_KHR_multiview`)**
  - Single-pass stereo/VR cluster culling in task shaders and meshlet output routing with `gl_ViewIndex`.
- `assignment48_timeline_semaphore_batch_graph/`: **Assignment 48 – Timeline Semaphore Batch Graph Scheduler (`VK_KHR_timeline_semaphore`)**
  - Monotonic 64-bit GPU-to-GPU dependency orchestration across Graphics, Compute, and Transfer queue families.
- `assignment49_low_latency_swapchain_timing/`: **Assignment 49 – Low Latency Swapchain Timing & Latency Sleep (`VK_NV_low_latency2`)**
  - Sub-millisecond click-to-photon latency minimization and dynamic CPU-GPU pacing with `vkLatencySleepNV`.
- `assignment50_ray_tracing_callable_shaders/`: **Assignment 50 – Ray Tracing Callable Shaders & Procedural BRDFs (`VK_KHR_ray_tracing_pipeline`)**
  - Dynamic polymorphic material evaluation in Closest-Hit and Miss shaders via `executeCallableKHR`.
- `assignment51_dma_sparse_residency_streaming/`: **Assignment 51 – Direct Linear DMA Staging & Sparse Residency (`vkQueueBindSparse`)**
  - Gigabyte-scale virtual texture management with asynchronous $64\text{KB}$ physical tile commitments.
- `assignment52_cooperative_vector_tensor_filtering/`: **Assignment 52 – Cooperative Matrix & Vector Tensor Filtering (`VK_KHR_cooperative_matrix`)**
  - Hardware Tensor Core matrix multiply-accumulate across subgroup wave lanes for real-time denoising.
- `assignment53_ray_tracing_position_fetch/`: **Assignment 53 – Ray Tracing Position Fetch & BVH Extraction (`VK_KHR_ray_tracing_position_fetch`)**
  - Extracting 3D vertex positions and normals directly from BVH geometry leaves without vertex buffer descriptors.
- `assignment54_device_diagnostic_checkpoints/`: **Assignment 54 – Device Diagnostic Checkpoints & Fault Recovery (`VK_NV_device_diagnostic_checkpoints` / `VK_EXT_device_fault`)**
  - Breadcrumb checkpoint markers and crash dump registers for isolating GPU Device Lost and hardware TDRs.
- `assignment55_saliency_shading_rate_maps/`: **Assignment 55 – Saliency Shading Rate Maps & Dynamic Foveated VRS (`VK_KHR_fragment_shading_rate`)**
  - Compute-generated $R8\_UINT$ density maps and attachment shading rate bindings for 40%+ fill-rate savings.
- `assignment56_dgc_token_multi_pipeline_draws/`: **Assignment 56 – Dynamic Multi-Draw Shader Indirect with Graphics Pipeline Tokens (`VK_EXT_device_generated_commands`)**
  - Autonomous GPU-driven execution changing pipeline states, push constants, and draw calls on device.
- `assignment57_hdr_color_space_metadata/`: **Assignment 57 – High Dynamic Range (HDR10) Color Space Management (`VK_EXT_hdr_metadata`)**
  - BT.2020 wide color gamut, ST.2084 Perceptual Quantizer encoding, and mastering display luminance metadata.
- `assignment58_vulkan_video_hardware_decode/`: **Assignment 58 – Zero-Copy Video Decoding & Vulkan Video Integration (`VK_KHR_video_queue`)**
  - Dedicated hardware video decoding (H.264/H.265) and direct shader sampling via `VkSamplerYcbcrConversion`.
- `assignment59_memory_model_queue_transfers/`: **Assignment 59 – Memory Model Queue Ownership Transfers (`VK_KHR_vulkan_memory_model`)**
  - Lock-free GPU ring buffers and matching release/acquire barriers across asynchronous queue families.
- `assignment60_indirect_ray_tracing_dispatch/`: **Assignment 60 – Dynamic Graph Execution & Indirect Ray Tracing (`vkCmdTraceRaysIndirectKHR`)**
  - GPU-computed adaptive ray tracing dispatch budgets with zero CPU command recording overhead.
- `assignment61_ray_tracing_motion_blur_matrices/`: **Assignment 61 – Ray Tracing Partitioned Motion & Matrix Blur (`VK_NV_ray_tracing_motion_blur`)**
  - Multi-matrix interpolated motion blur, temporal ray sampling $[t_{min}, t_{max}]$, and Motion BLAS/TLAS hierarchies.
- `assignment62_subgroup_cluster_operations/`: **Assignment 62 – Shader Core Builtins & Subgroup Cluster Operations (`VK_KHR_shader_subgroup_clustered`)**
  - Intra-wave SIMD clustering ($K \in \{2, 4, 8, 16, 32\}$), hierarchical segmented parallel scans, and bank-conflict-free wave math.
- `assignment63_multi_draw_indirect_draw_parameters/`: **Assignment 63 – Hardware Primitive Topologies & Multi-Draw Indirect with Draw Parameters (`VK_KHR_shader_draw_parameters`)**
  - Direct draw metadata access via `gl_DrawID`, `gl_BaseVertex`, and `gl_BaseInstance` for zero-CPU batched sub-mesh dispatches.
- `assignment64_present_timing_frame_pacing/`: **Assignment 64 – Advanced Frame Pacing, Present Timing & Swapchain Feedback (`VK_EXT_present_timing`)**
  - Nanosecond present timing telemetry, future V-Sync refresh cycle quantization, and micro-judder elimination.
- `assignment65_async_compute_physics_graphics/`: **Assignment 65 – Multi-Queue Direct Compute Physics & Graphics Asynchronous Pipeline**
  - Independent simulation on dedicated compute queues concurrently feeding graphics queues via 64-bit timeline semaphore signaling.
- `assignment66_rasterization_order_subpass_shading/`: **Assignment 66 – Programmable Rasterization Order & Subpass Shading (`VK_EXT_rasterization_order_attachment_access`)**
  - In-order fragment shader read-modify-write without subpass barriers for deterministic Order-Independent Transparency (OIT) & custom blending.
- `assignment67_mesh_shading_multi_topologies/`: **Assignment 67 – Hardware Mesh Shading with Dual Primitive Topologies & Multi-Resolution Meshlets (`VK_EXT_mesh_shader`)**
  - Dynamic topology compaction (strips/points/triangles), per-primitive attributes, and screen-space adaptive meshlet tessellation.
- `assignment68_custom_gpu_memory_allocator/`: **Assignment 68 – Direct Memory Addressing, Custom Allocators & Suballocated Device Memory**
  - Free-List / Buddy GPU Memory Allocator architecture, large slab suballocations ($128\text{MB}$+), BDA alignment, and dedicated allocation optimizations.
- `assignment69_acceleration_structure_compaction_serialization/`: **Assignment 69 – Hardware Acceleration Structure Serialization, Deserialization & Compaction (`VK_KHR_ray_tracing_pipeline`)**
  - Post-build compacted size queries (`VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR`), 40%+ BVH memory compaction, and binary disk serialization/reloading.
- `assignment70_autonomous_gpu_driven_engine/`: **Assignment 70 – Comprehensive Autonomous GPU-Driven Rendering Engine (DGC + Mesh Shaders + Indirect RT + Dynamic Rendering)**
  - Full synthesis: DGC multi-pipeline dispatch, task/mesh shader clusters, inline ray-queried shadows, and zero-CPU draw loop execution.
- `assignment71_nanite_software_rasterizer/`: **Assignment 71 – Nanite-Style Micro-Polygon Software Rasterizer via 64-bit Atomics (`VK_KHR_shader_atomic_int64` / `VK_EXT_shader_atomic_float2`)**
  - Compute shader 2D edge-equation software rasterization, 64-bit atomic visibility buffer (`uint64_t(depth << 32 | tri_id)`), and fullscreen dynamic rendering material resolve.
- `assignment72_low_latency_reflex_pacing/`: **Assignment 72 – Ultra-Low-Latency Reflex Pacing & Input-to-Photon Instrumentation (`VK_NV_low_latency2` / `VK_EXT_present_timing`)**
  - Fine-grained low latency CPU-GPU pacing markers (`INPUT_SAMPLE`, `SIMULATION_START/END`, `RENDERSUBMIT_START/END`) and dynamic rendering frame pacing.
- `assignment73_fragment_barycentrics_wireframe/`: **Assignment 73 – Hardware Fragment Barycentrics & Analytic Wireframe Anti-Aliasing (`VK_KHR_fragment_shader_barycentric`)**
  - Single-pass dynamic wireframe rendering using hardware `gl_BaryCoordEXT` and screen-space partial derivative width `fwidth(bary)`.
- `assignment74_device_fault_bda_telemetry/`: **Assignment 74 – Device Address Binding Reporting & Post-Mortem GPU Page-Fault Telemetry (`VK_EXT_device_address_binding_report` / `VK_EXT_device_fault`)**
  - 64-bit Buffer Device Address raw pointer dereferencing (`GL_EXT_buffer_reference2`) with host-side memory interval registration and fault triage telemetry.
- `assignment75_shader_tile_image_deferred/`: **Assignment 75 – Tile-Local Subpass Operations & Dynamic Shading via Tile Image (`VK_EXT_shader_tile_image`)**
  - Zero-bandwidth G-Buffer MRT rasterization and tile-local deferred lighting resolve within on-chip tile memory.
- `assignment76_maintenance7_workgroup_specialization/`: **Assignment 76 – Maintenance 7 Dynamic Workgroup Specialization (`VK_KHR_maintenance7`)**
  - Subgroup limit introspection and dynamic workgroup size specialization constants (`local_size_x_id`) driving turbulent particle physics simulation.
- `assignment77_ray_tracing_curve_swept_spheres/`: **Assignment 77 – Ray Tracing Swept Spheres & Curve Primitives (`VK_NV_ray_tracing_linear_swept_spheres`)**
  - Hardware linear swept sphere (LSS) strand primitives, continuous 3D fiber tube generation, and dynamic Blinn-Phong shading.
- `assignment78_render_graph_dag_transpiler/`: **Assignment 78 – Multi-Queue Timeline Render Graph with Automatic Synchronization2 Transpiler**
  - Directed Acyclic Graph (DAG) topological pass compiler with automated RAW/WAR hazard detection and zero-manual `VkDependencyInfo` barrier emission.
- `assignment79_optical_flow_motion_vectors/`: **Assignment 79 – Hardware Optical Flow Vector Estimation & Temporal Reprojection (`VK_NV_optical_flow`)**
  - Dense screen-space 2D velocity fields, temporal reprojection delta tracking, and chromatic HSV motion field visualization.
- `assignment80_ultimate_mega_engine_capstone/`: **Assignment 80 – The Ultimate Autonomous Vulkan 1.4 Unified Mega-Engine Capstone**
  - Master synthesis: 64-bit BDA descriptorless geometry, Synchronization2 pipeline hazard barriers, Dynamic Rendering multi-light PBR forward shading, and zero-CPU descriptor pool binding.


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

