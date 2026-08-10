# Assignment 10 – Buffer Device Address and Zero-Copy Streaming

## Overview
Stream dynamic vertex data directly into host-visible, device-local GPU memory using raw buffer GPU pointers via `VK_KHR_buffer_device_address` without descriptors or staging buffers.

## Key Concepts
- `VK_KHR_buffer_device_address` (Vulkan 1.2+ core) & `vkGetBufferDeviceAddress`.
- Shader reference pointers via `GL_EXT_buffer_reference`.
- ReBAR host-visible + device-local memory allocation (`HOST_VISIBLE | DEVICE_LOCAL`).
- Circular ring-buffering & fence CPU/GPU synchronization.
