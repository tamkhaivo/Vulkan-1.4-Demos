# Assignment 20 – Sparse Virtual Texturing & Residency Streaming (`sparseResidencyImage2D`)

## Overview & Architectural Critique
Rendering ultra-high-resolution textures (e.g. $16\text{K}\times 16\text{K}$ gigapixel terrain or megatextures) exceeds physical VRAM budgets if loaded entirely.

In Vulkan 1.4, **Sparse Resources & Sparse Virtual Texturing (SVT)** allows allocating images with virtual memory address space where physical memory pages (typically $64\text{KB}$ tiles) are committed dynamically on demand using `vkQueueBindSparse`. In shaders, texel residency is queried via `sparseTextureARB()` / `sparseImageLoad()`, enabling background streaming of missing mip tiles without stalling the GPU pipeline.

## Key Vulkan 1.4 Concepts
- **Sparse Residency Device Features**: `sparseBinding = VK_TRUE`, `sparseResidencyImage2D = VK_TRUE`.
- **Querying Sparse Requirements**: `vkGetImageSparseMemoryRequirements` determining `imageGranularity` ($W \times H \times D$), `mipTailFirstLod`, and `mipTailSize`.
- **Asynchronous Sparse Binding**: Recording page table commits on a sparse queue via `vkQueueBindSparse` with `VkSparseImageMemoryBindInfo`.
- **Resident Mip-Tail**: Committing the single contiguous block of physical memory for mips smaller than one standard sparse tile.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Sparse Image
VkImageCreateInfo sparseImageInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .extent = { 16384, 16384, 1 }, // 16K Virtual Texture
    .mipLevels = 15,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
};
vkCreateImage(device, &sparseImageInfo, nullptr, &sparseImage);

// 2. Query Sparse Memory Requirements
uint32_t sparseReqCount = 0;
vkGetImageSparseMemoryRequirements(device, sparseImage, &sparseReqCount, nullptr);
std::vector<VkSparseImageMemoryRequirements> sparseReqs(sparseReqCount);
vkGetImageSparseMemoryRequirements(device, sparseImage, &sparseReqCount, sparseReqs.data());

// 3. Commit Physical Tile Page on Demand via vkQueueBindSparse
VkSparseImageMemoryBind tileBind{
    .subresource = { VK_IMAGE_ASPECT_COLOR_BIT, targetMipLevel, 0 },
    .offset = { tileX * tileWidth, tileY * tileHeight, 0 },
    .extent = { tileWidth, tileHeight, 1 },
    .memory = physicalMemoryChunk,
    .memoryOffset = physicalChunkOffset,
    .flags = 0
};

VkSparseImageMemoryBindInfo imageBindInfo{
    .image = sparseImage,
    .bindCount = 1,
    .pBinds = &tileBind
};

VkBindSparseInfo bindSparseInfo{
    .sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
    .imageBindCount = 1,
    .pImageBinds = &imageBindInfo
};

// Asynchronously update GPU page tables on sparse queue
vkQueueBindSparse(sparseQueue, 1, &bindSparseInfo, sparseCommitFence);
```

## Acceptance Criteria
- [x] Query and verify `sparseResidencyImage2D` support on the physical device.
- [x] Create a $16384\times 16384$ sparse image with `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT`.
- [x] Extract `VkSparseImageMemoryRequirements` and calculate tile dimensions and mip-tail layout.
- [x] Bind the mip-tail chunk and commit active visible $64\text{KB}$ physical memory pages using `vkQueueBindSparse`.
- [x] Sample the sparse virtual texture in fragment shaders with smooth dynamic streaming and zero validation errors.

## Directory Structure
- `src/main.cpp`: Sparse virtual texturing host application.
- `shaders/sparse_terrain.vert`, `shaders/sparse_terrain.frag`: Residency query shaders.
- `CMakeLists.txt`: Build target configuration.
