# Assignment 81 – Zero-Copy AV1 Hardware Video Decoding & YCbCr Sampler Feedback

## Overview & Architectural Critique
Streaming real-time video textures without CPU bottlenecks requires direct on-die silicon decoders. Traditional implementations copy decompressed planar NV12/YUV buffers to CPU and re-upload them to RGBA images, wasting memory bus bandwidth. **Assignment 81** implements `VK_KHR_video_decode_av1` and `VK_KHR_sampler_ycbcr_conversion`, feeding hardware-decoded AV1 video bitstreams directly into GPU memory with zero CPU readbacks.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_video_queue` & `VK_KHR_video_decode_av1`**: Video session configuration.
- **Zero-Copy Bitstream Decoding**: Passing raw AV1 compressed chunks directly into GPU hardware decoders.
- **`VkSamplerYcbcrConversion`**: Hardware color-space conversion in shaders.
- **Dynamic Rendering**: Rendering decoded frames onto 3D surfaces.

## Acceptance Criteria
- [x] Query and configure hardware video decode capabilities.
- [x] Create TV/display geometry with dynamic UV coordinates.
- [x] Render animated AV1-decoded dynamic test patterns using dynamic rendering.
- [x] 0 validation layer errors.

## Directory Structure
- `src/main.cpp`: Video decoding runtime and dynamic rendering pipeline.
- `shaders/av1_video.vert`, `shaders/av1_video.frag`: YCbCr dynamic conversion shaders.
- `CMakeLists.txt`: Build target configuration.
