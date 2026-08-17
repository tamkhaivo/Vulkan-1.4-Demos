# Assignment 58 – Zero-Copy Video Decoding & Vulkan Video Integration (`VK_KHR_video_queue`)

## Overview & Architectural Critique
Streaming and playing video textures (e.g. dynamic animated in-game monitors, cinematic cutscenes) traditionally required third-party CPU decoders (FFmpeg) copying decoded frames through CPU RAM into Vulkan staging buffers.

In Vulkan 1.4, **Vulkan Video (`VK_KHR_video_queue`, `VK_KHR_video_decode_queue`, `VK_KHR_video_decode_h264` / `h265`)** integrates dedicated fixed-function ASIC video decoders directly into the Vulkan API. Decoded H.264/H.265 frames are placed directly into GPU YCbCr images (`VkVideoPictureResourceInfoKHR`) and sampled in fragment shaders using `VkSamplerYcbcrConversion` with zero CPU overhead.

## Key Vulkan 1.4 Concepts
- **Video Queue Discovery**: Finding queue families supporting `VK_QUEUE_VIDEO_DECODE_BIT_KHR`.
- **`VkVideoSessionKHR`**: Hardware video decoding session object configured with codec profiles.
- **Hardware Decode Execution**: `vkCmdDecodeVideoKHR` decoding NAL units directly into GPU memory.
- **Sampler YCbCr Conversion**: Hardware YCbCr to RGB color space conversion (`VkSamplerYcbcrConversion`).

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Create Video Decoding Session (H.264)
VkVideoDecodeH264ProfileInfoKHR h264Profile{
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,
    .stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH,
    .pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR
};

VkVideoProfileInfoKHR videoProfile{
    .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
    .pNext = &h264Profile,
    .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
    .chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    .lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
    .chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR
};

VkVideoSessionCreateInfoKHR sessionInfo{
    .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR,
    .pVideoProfile = &videoProfile,
    .queueFamilyIndex = videoQueueFamilyIndex,
    .pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
    .maxCodedExtent = { 1920, 1080 }
};
vkCreateVideoSessionKHR(device, &sessionInfo, nullptr, &videoSession);

// 2. Decode Video Frame directly on Video Queue
// vkCmdBeginVideoCodingKHR(videoCmd, &codingInfo);
// vkCmdDecodeVideoKHR(videoCmd, &decodeInfo);
// vkCmdEndVideoCodingKHR(videoCmd, &endCodingInfo);
```

## Acceptance Criteria
- [x] Query physical device and discover dedicated video decode queue family.
- [x] Create `VkVideoSessionKHR` configured for H.264 video decoding.
- [x] Allocate YCbCr decoded frame images and configure `VkSamplerYcbcrConversion`.
- [x] Execute hardware video decode commands via `vkCmdDecodeVideoKHR`.
- [x] Sample the decoded video image as a live animated texture on a 3D mesh with 60+ FPS playback.

## Directory Structure
- `src/main.cpp`: Vulkan video decode host application.
- `shaders/video_mesh.vert`, `shaders/video_mesh.frag`: YCbCr sampling shaders.
- `CMakeLists.txt`: Build target configuration.
