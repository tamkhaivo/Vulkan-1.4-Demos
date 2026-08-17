# Assignment 58 – Zero-Copy Video Decoding & Vulkan Video Integration (`VK_KHR_video_queue` / `VK_KHR_video_decode_h264`)

## Overview
Stream video content directly into 2D Vulkan textures without CPU decoding or staging memory copies using hardware-accelerated Vulkan Video extensions (`VK_KHR_video_queue`, `VK_KHR_video_decode_queue`, `VK_KHR_video_decode_h264`).

## Key Concepts
- Video session creation (`VkVideoSessionKHR`, `VkVideoProfileInfoKHR`).
- Allocating dedicated video memory with `vkGetVideoSessionMemoryRequirementsKHR`.
- Recording hardware decode commands (`vkCmdBeginVideoCodingKHR`, `vkCmdDecodeVideoKHR`, `vkCmdEndVideoCodingKHR`).
- Zero-copy sampling of decoded YCbCr frames in graphics shaders via `VkSamplerYcbcrConversion`.

## Acceptance Criteria
- [x] Query physical device video decoding capabilities for H.264/H.265.
- [x] Initialize `VkVideoSessionKHR` and allocate required video picture profile buffers.
- [x] Record video decoding commands with `vkCmdDecodeVideoKHR`.
- [x] Sample decoded frame textures directly in a graphics fragment pass with YCbCr sampler conversions.
- [x] Verify zero CPU decode overhead and smooth 60fps presentation.

## Directory Structure
- `src/main.cpp`: Vulkan Video session and decode pipeline host application.
- `CMakeLists.txt`: Build target configuration.
