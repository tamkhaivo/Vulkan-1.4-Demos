# Assignment 29 – Host Image Copy & Zero-Staging Direct Uploads (`VK_EXT_host_image_copy`)

## Overview
Perform direct host-to-device image pixel uploads and layout transitions on the CPU without allocating intermediate staging buffers or recording queue command submissions.

## Key Concepts
- `VK_EXT_host_image_copy` / Vulkan 1.4 core feature enablement (`hostImageCopy`).
- Direct CPU memory to GPU image copying via `vkCopyMemoryToImageEXT`.
- Host-side image layout transitions using `vkTransitionImageLayoutEXT`.
- Eliminating staging buffer allocation, copy command recording, and queue submission overhead for CPU texture streaming.

## Acceptance Criteria
- [x] Enable `hostImageCopy` in `VkPhysicalDeviceHostImageCopyFeaturesEXT`.
- [x] Create an image with `VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT` flag enabled.
- [x] Transition image layout on the host via `vkTransitionImageLayoutEXT` (e.g. `UNDEFINED` -> `SHADER_READ_ONLY_OPTIMAL`).
- [x] Upload raw pixel memory directly to the GPU image with `vkCopyMemoryToImageEXT`.
- [x] Sample the uploaded texture directly in shaders with validation layers reporting clean execution.

## Directory Structure
- `src/main.cpp`: Host image copy host application.
- `shaders/`: GLSL shaders for texture rendering.
- `CMakeLists.txt`: Build target configuration.
