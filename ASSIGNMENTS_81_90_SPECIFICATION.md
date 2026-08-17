# Advanced Vulkan 1.4 Curriculum Expansion: Assignments 81 – 90

This specification details **10 next-generation, high-performance Vulkan 1.4 assignments** extending beyond the foundational capstone (Assignments 1–80). These assignments target bleeding-edge Vulkan 1.4 core specifications, newly ratified Khronos KHR extensions, and specialized vendor hardware extensions (AV1 decode/encode, cooperative vector matrix tensor math, cluster acceleration structures, device fault triage, fine-grained host memory, programmable subpass raster orders, and sparse bindless residency).

---

## Table of Contents
1. [Assignment 81: Zero-Copy AV1 Hardware Video Decoding & YCbCr Sampler Feedback](#assignment-81-zero-copy-av1-hardware-video-decoding--ycbcr-sampler-feedback)
2. [Assignment 82: Sub-Group Matrix Tensor Convolutions & Direct Neural Filtering](#assignment-82-sub-group-matrix-tensor-convolutions--direct-neural-filtering)
3. [Assignment 83: Dynamic Fragment Density Maps & Eye-Tracked Foveated Shading](#assignment-83-dynamic-fragment-density-maps--eye-tracked-foveated-shading)
4. [Assignment 84: Clustered Level Acceleration Structures (CLAS) for Micro-Mesh BVH Compaction](#assignment-84-clustered-level-acceleration-structures-clas-for-micro-mesh-bvh-compaction)
5. [Assignment 85: Direct GPU Memory Fault Triage & Page-Fault Telemetry](#assignment-85-direct-gpu-memory-fault-triage--page-fault-telemetry)
6. [Assignment 86: Sparse Dynamic Multi-Layer Virtual Megatexturing with Residency Feedback](#assignment-86-sparse-dynamic-multi-layer-virtual-megatexturing-with-residency-feedback)
7. [Assignment 87: Fine-Grained Programmable Raster Order Attachment Access & Lock-Free OIT](#assignment-87-fine-grained-programmable-raster-order-attachment-access--lock-free-oit)
8. [Assignment 88: Dynamic Multi-Queue Async Physics & Compute-to-Direct-Meshlet Stream](#assignment-88-dynamic-multi-queue-async-physics--compute-to-direct-meshlet-stream)
9. [Assignment 89: Direct Host Memory Image Blits & ReBAR Zero-Copy Texture Streaming](#assignment-89-direct-host-memory-image-blits--rebar-zero-copy-texture-streaming)
10. [Assignment 90: Master Autonomous Heterogeneous Engine Capstone II](#assignment-90-master-autonomous-heterogeneous-engine-capstone-ii)

---

## Assignment 81: Zero-Copy AV1 Hardware Video Decoding & YCbCr Sampler Feedback

### Overview & Architectural Critique
Streaming real-time video textures (security streams, in-game dynamic billboards, cutscenes) without CPU bottlenecks requires direct on-die silicon decoders. Traditional implementations copy decompressed planar NV12/YUV buffers to CPU and re-upload them to RGBA images, wasting hundreds of megabytes/sec in memory bus bandwidth. **Assignment 81** implements `VK_KHR_video_decode_av1` and `VK_KHR_sampler_ycbcr_conversion`, feeding hardware-decoded AV1 video bitstreams directly into GPU memory with zero CPU readbacks.

### Key Vulkan 1.4 Concepts
- **`VK_KHR_video_queue` & `VK_KHR_video_decode_av1`**: Initializing `VkVideoSessionKHR` with AV1 decode profiles (`VkVideoDecodeAV1ProfileInfoKHR`).
- **Zero-Copy Bitstream Decoding**: Passing raw AV1 compressed frame chunks into `vkCmdDecodeVideoKHR`.
- **`VkSamplerYcbcrConversion`**: Single-pass multi-planar $Y'CbCr$ 4:2:0 to linear $sRGB$ color space transform in fragment shaders.
- **Synchronization2 Video Pipeline Barriers**: `VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR` transitions to `VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT`.

### Acceptance Criteria
- [ ] Query physical device video queue families supporting `VK_QUEUE_VIDEO_DECODE_BIT_KHR`.
- [ ] Initialize `VkVideoProfileInfoKHR` with AV1 decoding flags and create `VkVideoSessionKHR`.
- [ ] Record hardware decode commands via `vkCmdDecodeVideoKHR` into dedicated video command buffers.
- [ ] Sample decoded DPB (Decoded Picture Buffer) slices using `VkSamplerYcbcrConversion` directly on rotating 3D mesh surfaces.
- [ ] Zero validation layer errors and stable 60+ FPS playback.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 81: Vulkan 1.4 AV1 Zero-Copy Video Decoding]                       |
+--------------------------------------------------------------------------------+
|  +---------------------------+        +-------------------------------------+  |
|  | AV1 Bitstream (Annex B)   |        | 3D Textured Surface (Y'CbCr -> RGB) |  |
|  | [Keyframe + Inter-Frames] |        |                                     |  |
|  +-------------+-------------+        |        .------------------.         |  |
|                |                      |       /  VIDEO BILLBOARD /|         |  |
|                v                      |      /  [AV1 Decoded]   / |         |  |
|  +---------------------------+        |     +------------------+  |         |  |
|  | Hardware AV1 Silicon      |------->|     |  * Real-Time *   |  |         |  |
|  | (VkVideoSessionKHR)       | Zero-  |     |  * 4K Playback * |  /         |  |
|  | DPB Memory Array          | Copy   |     |  * Zero CPU Copy*| /          |  |
|  +---------------------------+        |     +------------------+/           |  |
|                                       +-------------------------------------+  |
|  Telemetry: Decode Time: 0.32ms | Bus Bandwidth: 0.0 MB/s CPU-GPU Transfer     |
+--------------------------------------------------------------------------------+
```

---

## Assignment 82: Sub-Group Matrix Tensor Convolutions & Direct Neural Filtering

### Overview & Architectural Critique
Post-processing passes such as bilateral upscaling, neural super-resolution, and path-tracing denoising are traditionally bound by scalar fragment shader compute limits. Leveraging hardware Tensor/Matrix cores directly in compute pipelines enables dense FP16 matrix-multiply-accumulate (MMA) operations with over $5\times$ ALU throughput. **Assignment 82** uses `VK_KHR_cooperative_matrix` to implement a high-throughput $16 \times 16$ tile neural edge-preserving filter directly across subgroup wave invocations.

### Key Vulkan 1.4 Concepts
- **`VK_KHR_cooperative_matrix`**: Introspecting matrix sizes ($M, N, K$) and supported types (`float16_t`, `bfloat16_t`, `int8_t`).
- **Wave-Level Cooperative Instructions**: `coopmat<float16_t, gl_ScopeSubgroup, 16, 16, gl_MatrixUseA>`, `coopMatLoad`, `coopMatMulAdd`, and `coopMatStore`.
- **Workgroup Shared Memory Tiling**: Cache-aligned tile loads feeding cooperative matrix arithmetic units across 32/64-wide subgroups.

### Acceptance Criteria
- [ ] Query and select compatible cooperative matrix configurations ($M=16, N=16, K=16$, ComponentType = Float16).
- [ ] Construct a compute dispatch performing $3\times 3$ separable neural convolution kernel filtering on HDR render targets.
- [ ] Execute `coopMatMulAdd` inside compute workgroups without scalar bank conflicts.
- [ ] Output dynamic before/after side-by-side denoised output.
- [ ] Validate 100% adherence to SPIR-V Cooperative Matrix semantics with 0 validation errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 82: Subgroup Cooperative Matrix Neural Filtering]                  |
+--------------------------------------------------------------------------------+
|  RAW NOISY INPUT (Left)              |  TENSOR DENOISED OUTPUT (Right)         |
|  ::::::::::::::::::::::::::::::::    |  ================================       |
|  :::::  ..:::::  .:::::::::::::::    |  |######|           |########|          |
|  :::   .:::::::::   :::::::::::::    |  |######|  Smooth   |########|          |
|  :::::..:::::::::::..::::::::::::    |  |######| Surfaces  |########|          |
|  ::::::::::::::::::::::::::::::::    |  ================================       |
|                                      |                                         |
|  [Wave Subgroup MMA: 16x16 Float16]  |  Subgroup Execution: 0.18ms / Frame     |
+--------------------------------------------------------------------------------+
```

---

## Assignment 83: Dynamic Fragment Density Maps & Eye-Tracked Foveated Shading

### Overview & Architectural Critique
High-resolution 4K and XR displays strain GPU fragment fill-rates on peripheral geometry where visual acuity is minimal. Variable Rate Shading (VRS) combined with dynamic foveation maps dynamically relaxes fragment shading rates from $1\times 1$ to $4\times 4$ depending on fixation point coordinates. **Assignment 83** implements dynamic attachment shading rate maps using `VK_KHR_fragment_shading_rate` with dynamic gaze coordinates updated every frame.

### Key Vulkan 1.4 Concepts
- **Attachment-Driven VRS**: Binding an $R8\_UINT$ texture view to `VkRenderingAttachmentInfo.pNext` using `VkRenderingFragmentShadingRateAttachmentInfoKHR`.
- **Dynamic Foveated Density Generation**: Compute pass generating radial Gaussian shading rate distributions ($1\times 1$ fovea center, $2\times 2$ mid-periphery, $4\times 4$ far periphery).
- **Shading Rate Combiner Ops**: Combining pipeline state shading rates with attachment rate maps via `VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR`.

### Acceptance Criteria
- [ ] Configure `VkPhysicalDeviceFragmentShadingRateFeaturesKHR` and query `minFragmentShadingRateAttachmentTexelSize`.
- [ ] Compute real-time dynamic foveation rate map buffers linked to cursor/gaze movement.
- [ ] Attach VRS density maps to dynamic rendering passes (`vkCmdBeginRendering`).
- [ ] Measure and display fill-rate performance gain ($>35\%$ fragment invocation reduction).
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 83: Dynamic Foveated Variable Rate Shading]                        |
+--------------------------------------------------------------------------------+
|     +--------------------------------------------------------------------+     |
|     |  [4x4 Rate: Coarse Outer]                                          |     |
|     |        +--------------------------------------------------+        |     |
|     |        |  [2x2 Rate: Intermediate Blend]                  |        |     |
|     |        |        +--------------------------------+        |        |     |
|     |        |        |   [1x1 Rate: Pinpoint Fovea]   |        |        |     |
|     |        |        |          (Gaze Target)         |        |        |     |
|     |        |        |                *               |        |        |     |
|     |        |        +--------------------------------+        |        |     |
|     |        +--------------------------------------------------+        |     |
|     +--------------------------------------------------------------------+     |
|  Fovea Center: (X: 0.52, Y: 0.48) | Fragment Work Reduction: 44.2%             |
+--------------------------------------------------------------------------------+
```

---

## Assignment 84: Clustered Level Acceleration Structures (CLAS) for Micro-Mesh BVH Compaction

### Overview & Architectural Critique
Traditional Bottom-Level Acceleration Structures (BLAS) rebuilds for procedural, deforming, or highly detailed micro-polygon meshes cause massive CPU/GPU synchronization bubbles. **Assignment 84** implements `VK_NV_cluster_acceleration_structure`, introducing a hierarchical subdivision where meshes are grouped into independent clusters ($128$ to $512$ triangles) that can be individually pruned, updated, and compacted in sub-milliseconds without whole-BLAS invalidation.

### Key Vulkan 1.4 Concepts
- **Cluster Acceleration Structures (CLAS)**: Building fine-grained leaf cluster structures with `VkClusterAccelerationStructureInputInfoNV`.
- **Dynamic Cluster Compaction**: Querying cluster bounding boxes directly from compute shaders and pruning back-facing/occluded clusters.
- **Hardware Traversal Efficiency**: Direct hardware Ray Tracing pipeline traversal through CLAS-enabled TLAS/BLAS hierarchies.

### Acceptance Criteria
- [ ] Enable `VK_NV_cluster_acceleration_structure` device extension and verify physical device limits.
- [ ] Partition a dense procedural terrain mesh ($1M+$ triangles) into 256-triangle geometric clusters.
- [ ] Execute GPU cluster culling and emit compacted CLAS structures on the device timeline.
- [ ] Trace inline ray queries (`rayQueryEXT`) against CLAS geometry.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 84: Clustered Acceleration Structure (CLAS) Micro-Meshes]          |
+--------------------------------------------------------------------------------+
|  TOP-LEVEL ACCELERATION STRUCTURE (TLAS)                                       |
|     |                                                                          |
|     v                                                                          |
|  BOTTOM-LEVEL ACCELERATION STRUCTURE (BLAS)                                    |
|     |---> Cluster #001 [256 Tris] (Active - Traced)   [======]                 |
|     |---> Cluster #002 [256 Tris] (Active - Traced)   [======]                 |
|     |---> Cluster #003 [256 Tris] (Pruned / Compacted)[ xxxx ]                 |
|     `---> Cluster #004 [256 Tris] (Active - Traced)   [======]                 |
|                                                                                |
|  Rebuild Time: 0.12 ms (vs 4.80 ms Standard BLAS) | Memory Savings: 62%        |
+--------------------------------------------------------------------------------+
```

---

## Assignment 85: Direct GPU Memory Fault Triage & Page-Fault Telemetry

### Overview & Architectural Critique
When asynchronous GPU compute kernels dereference out-of-bounds 64-bit Buffer Device Addresses (BDA), hardware engines trigger catastrophic TDRs (Timeout Detection and Recovery) or `VK_ERROR_DEVICE_LOST` with zero CPU callstack fidelity. **Assignment 85** integrates `VK_EXT_device_fault` and `VK_EXT_device_address_binding_report` to create a real-time GPU hardware telemetry and crash dump triage harness that isolates fault addresses to exact resource names and offsets.

### Key Vulkan 1.4 Concepts
- **`VK_EXT_device_fault`**: Querying `vkGetDeviceFaultInfoEXT` for address fault registers and hardware crash status.
- **`VK_EXT_device_address_binding_report`**: Registering debug callbacks for `VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT` to track 64-bit memory allocations.
- **Controlled Fault Injection**: Intentionally generating and safely intercepting an invalid BDA dereference inside a compute kernel.

### Acceptance Criteria
- [ ] Register memory interval tracking trees for all allocated BDA GPU buffers.
- [ ] Configure `VkDeviceFaultCountsEXT` and `VkDeviceFaultInfoEXT` structures.
- [ ] Execute a test harness compute shader with configurable invalid pointer dereferences.
- [ ] Intercept `VK_ERROR_DEVICE_LOST` and output the exact buffer name, size, and fault offset.
- [ ] Gracefully clean up Vulkan instance resources.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 85: GPU Hardware Fault & BDA Telemetry Report]                     |
+--------------------------------------------------------------------------------+
| [FAULT EVENT DETECTED] GPU ENGINE: Compute Pipe 0 | Code: ADDRESS_FAULT_PAGE   |
| Fault Virtual Address: 0x00007FFB3402A180                                      |
|                                                                                |
| Registered Resource Mapping:                                                   |
|   Resource: "Particle_Physics_SSBO_Buffer"                                     |
|   Base Address: 0x00007FFB34020000                                             |
|   Buffer Size:  0x0000000000008000 (32,768 Bytes)                             |
|   Offset Fault: +0x0000A180 (OUT-OF-BOUNDS BY 8,576 BYTES)                     |
|                                                                                |
| Status: Triage Telemetry Captured Successfully -> Driver Resumed Cleanly       |
+--------------------------------------------------------------------------------+
```

---

## Assignment 86: Sparse Dynamic Multi-Layer Virtual Megatexturing with Residency Feedback

### Overview & Architectural Critique
Open-world rendering engines require hundreds of gigabytes of unique landscape and surface textures that cannot fit into physical VRAM. **Assignment 86** builds a multi-layered Sparse Virtual Texture (SVT) megatexturing engine using `vkQueueBindSparse` and residency feedback buffers in fragment shaders, dynamically streaming $64\text{KB}$ physical memory pages on demand.

### Key Vulkan 1.4 Concepts
- **Sparse Virtual Residency**: Allocating sparse $16384 \times 16384$ texture images with `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` and `VK_IMAGE_CREATE_SPARSE_BINDING_BIT`.
- **GPU Residency Feedback Buffers**: Using `sparseImageSampleLodEXT` in GLSL to write missing tile coordinates into an SSBO feedback queue.
- **Asynchronous Tile Page Management**: Background worker thread allocating and binding physical `VkDeviceMemory` pages via `vkQueueBindSparse`.

### Acceptance Criteria
- [ ] Query `VkSparseImageMemoryRequirements` and configure tile granularity ($64\text{KB}$).
- [ ] Implement residency feedback collection inside the terrain fragment shader.
- [ ] Stream and bind new physical texture tiles dynamically as the camera navigates the scene.
- [ ] Maintain consistent 60 FPS without frame-drop hitches during tile streaming.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 86: Sparse Virtual Megatexturing (16K x 16K Virtual)]              |
+--------------------------------------------------------------------------------+
|  VIRTUAL TEXTURE PAGE TABLE (16384 x 16384)                                    |
|  +--------------------+--------------------+--------------------+           |
|  | [RESIDENT: Page 0] | [UNBOUND: Virtual] | [RESIDENT: Page 2] |           |
|  | (Physical VRAM)    | (Stream Pending)   | (Physical VRAM)    |           |
|  +--------------------+--------------------+--------------------+           |
|  | [RESIDENT: Page 3] | [RESIDENT: Page 4] | [RESIDENT: Page 5] |           |
|  +--------------------+--------------------+--------------------+           |
|                                                                                |
|  Tile Memory Allocated: 128 MB (Virtual Footprint: 2.1 GB) | Active Pages: 142 |
+--------------------------------------------------------------------------------+
```

---

## Assignment 87: Fine-Grained Programmable Raster Order Attachment Access & Lock-Free OIT

### Overview & Architectural Critique
Order-Independent Transparency (OIT) and per-pixel linked lists typically suffer from massive memory overheads and atomic contention. **Assignment 87** uses `VK_EXT_rasterization_order_attachment_access` to enable in-order read-modify-write access to color and depth attachments within a single subpass, producing deterministic multi-layer alpha blending without atomic locks or multi-pass sorting.

### Key Vulkan 1.4 Concepts
- **`VK_EXT_rasterization_order_attachment_access`**: Configuring `VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT`.
- **Programmable Blend Math**: Reading the current fragment target value, sorting depth layers in fragment register arrays, and computing exact front-to-back compositing.
- **Elimination of Fragment Collisions**: Hardware ensures overlapping primitive fragments execute sequentially without write hazards.

### Acceptance Criteria
- [ ] Enable rasterization order attachment features on color attachments.
- [ ] Render multiple intersecting semi-transparent colored glass surfaces.
- [ ] Execute per-pixel 4-layer sorting inside the fragment shader with zero atomic operations.
- [ ] Verify deterministic pixel color output invariant to triangle raster submission order.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 87: Rasterization Order Attachment Access (Lock-Free OIT)]         |
+--------------------------------------------------------------------------------+
|  INTERSECTING TRANSPARENT GEOMETRY                                             |
|                                                                                |
|         /========/ (Glass Layer 1: Red Alpha 0.5)                              |
|        /  /=====/==/ (Glass Layer 2: Green Alpha 0.5)                          |
|       /  /  /=====/==/ (Glass Layer 3: Blue Alpha 0.5)                         |
|      +--+--+-----+--+                                                          |
|      | Deterministic Compositing: [Layer 1 -> Layer 2 -> Layer 3]              |
|      | No Atomicity Latency | Zero Subpass Barriers | Exact Math               |
|      +-------------------------------------------------------+                 |
+--------------------------------------------------------------------------------+
```

---

## Assignment 88: Dynamic Multi-Queue Async Physics & Compute-to-Direct-Meshlet Stream

### Overview & Architectural Critique
Decoupling physics simulations from graphical frame rates prevents physics stutter during rendering spikes. **Assignment 88** executes a high-density cloth and rigid-body simulation on an asynchronous compute queue that streams updated vertex attributes directly into task and mesh shaders (`VK_EXT_mesh_shader`) via timeline semaphore ownership handoffs.

### Key Vulkan 1.4 Concepts
- **Asynchronous Cross-Queue Ownership**: Transferring SSBO ownership from dedicated Compute queue to Graphics queue using `VkBufferMemoryBarrier2`.
- **Timeline Semaphore Synchronization**: Monotonic 64-bit progress counters synchronizing variable physics ticks ($120\text{Hz}$) with graphics display ticks ($60\text{Hz}$).
- **Meshlet Direct Shading**: Task shaders consuming updated physics position buffers directly and emitting meshlet geometry.

### Acceptance Criteria
- [ ] Set up dedicated Compute and Graphics queues with independent command pools.
- [ ] Record cloth simulation compute dispatches ($128\times 128$ grid) on the Compute queue.
- [ ] Synchronize and transfer buffer access using timeline semaphores and `VkDependencyInfo`.
- [ ] Render animated cloth meshlets using `vkCmdDrawMeshTasksEXT`.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 88: Async Compute Physics to Direct Meshlet Stream]                |
+--------------------------------------------------------------------------------+
|  [COMPUTE QUEUE (120 Hz)]                                                      |
|  Simulate Cloth SSBO ---> Timeline Sem #101 ---> Buffer Release Barrier        |
|                                 |                                              |
|                                 v (Timeline Sync)                              |
|  [GRAPHICS QUEUE (60 Hz)]                                                      |
|  Buffer Acquire Barrier ---> Task Shader (Cull) ---> Mesh Shader (Draw Cloth)  |
|                                                                                |
|  Physics Timestep: 8.33ms | Render Frametime: 16.66ms | Lock-Free Overlap      |
+--------------------------------------------------------------------------------+
```

---

## Assignment 89: Direct Host Memory Image Blits & ReBAR Zero-Copy Texture Streaming

### Overview & Architectural Critique
On modern platforms with Resizable BAR (ReBAR) or unified memory architectures (Apple Silicon, APUs, modern discrete PCIe 4.0/5.0 GPUs), allocating intermediate staging buffers is redundant. **Assignment 89** leverages `VK_EXT_host_image_copy` combined with host-visible, device-local memory to stream dynamic procedural textures directly from CPU memory into optimal GPU tiled images with zero intermediate copies and zero command buffer recording overhead.

### Key Vulkan 1.4 Concepts
- **`VK_EXT_host_image_copy`**: Directly invoking `vkCopyMemoryToImageEXT` and `vkTransitionImageLayoutEXT` on the host CPU.
- **Host-Visible Device-Local Memory**: Detecting `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.
- **Elimination of Staging Buffers**: Bypassing `VkBuffer` allocations, command buffer allocation, queue submission, and fence waits for texture uploads.

### Acceptance Criteria
- [ ] Query and enable `VK_EXT_host_image_copy` features.
- [ ] Perform host-side image layout transitions (`vkTransitionImageLayoutEXT`) from `UNDEFINED` to `SHADER_READ_ONLY_OPTIMAL`.
- [ ] Upload real-time generated procedural noise textures directly to GPU image memory via `vkCopyMemoryToImageEXT`.
- [ ] Sample updated images seamlessly in the main dynamic rendering pass.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 89: Zero-Copy Host Image Direct Blit (ReBAR)]                      |
+--------------------------------------------------------------------------------+
|  HOST CPU MEMORY (Procedural Generator)                                        |
|  [Noise Buffer: 2048x2048 RGBA8]                                               |
|          |                                                                     |
|          | (vkCopyMemoryToImageEXT - Direct PCIe Bus DMA)                      |
|          v                                                                     |
|  GPU OPTIMAL TILING IMAGE (No Staging Buffer / No Command Queue Submission)    |
|  [VkImage: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL]                           |
|                                                                                |
|  Upload Latency: 0.28ms | Staging Buffer Memory: 0 MB                          |
+--------------------------------------------------------------------------------+
```

---

## Assignment 90: Master Autonomous Heterogeneous Engine Capstone II

### Overview & Architectural Critique
The ultimate synthesis of cutting-edge Vulkan 1.4 graphics engineering. **Assignment 90** builds a fully autonomous, heterogeneous engine combining:
1. **Device Generated Commands (DGC)** driving graphics pipelines dynamically on GPU.
2. **Hardware Ray Tracing with Motion Blur & Inline Queries** for dynamic lighting.
3. **Clustered Mesh/Task Shading with Subgroup Reductions** for procedural terrain and geometry.
4. **Synchronization2 DAG Render Graph & Dynamic Rendering** for multi-queue, multi-pass post-processing.
5. **Buffer Device Address (BDA) 64-bit Pointers** for complete descriptorless zero-pool memory binding.

### Key Vulkan 1.4 Concepts
- **Master Engine Architecture**: End-to-end integration of Vulkan 1.4 core and premier extensions.
- **GPU-Side Autonomous Execution**: Zero CPU draw calls or per-frame descriptor allocations.
- **Unified Memory & Shader Interface**: BDA pointers, dynamic state vectors, and timeline semaphore multi-queue orchestration.

### Acceptance Criteria
- [ ] Execute an autonomous rendering loop with 0 CPU draw calls per frame (GPU-driven DGC).
- [ ] Combine task/mesh shader clusters with inline ray-queried reflections and shadow tests.
- [ ] Orchestrate asynchronous compute, transfer, and graphics queues via 64-bit timeline semaphores.
- [ ] Deliver robust, validation-clean execution at 60+ FPS across thousands of dynamic procedural objects.
- [ ] 0 validation layer errors.

### Visual Example
```
+--------------------------------------------------------------------------------+
| [Assignment 90: Master Autonomous Vulkan 1.4 Heterogeneous Mega-Engine]        |
+--------------------------------------------------------------------------------+
|  +--------------------------------------------------------------------------+  |
|  | GPU AUTONOMOUS WORKFLOW (Zero CPU Dispatch Intervention)                 |  |
|  |                                                                          |  |
|  |  [Compute Culling] --> [DGC Token Stream] --> [Task & Mesh Shaders]      |  |
|  |                                                    |                     |  |
|  |                                                    v                     |  |
|  |  [Inline Ray Queries (Shadows)] <------- [Dynamic Rendering (Local Read)]|  |
|  |                                                    |                     |  |
|  |                                                    v                     |  |
|  |  [Tensor Core Neural Post-Process] <--- [DAG Auto-Synchronization2]      |  |
|  +--------------------------------------------------------------------------+  |
|  Frame Pacing: 144 FPS | CPU Draw Overhead: 0.00ms | Validation: 0 Errors      |
+--------------------------------------------------------------------------------+
```
