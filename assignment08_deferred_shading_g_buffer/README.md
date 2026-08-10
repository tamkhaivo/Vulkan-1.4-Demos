# Assignment 8 – Deferred Shading with Multiple Render Targets (Dynamic Rendering Local Reads)

## Overview
Implement deferred rendering: G-Buffer pass rendering Albedo, Normal, and Depth to multiple attachments, followed by a lighting pass reading all attachments via dynamic rendering local reads.

## Key Concepts
- Multiple Color Attachments in dynamic rendering.
- G-Buffer layout (`RGBA8`, `RGBA16_SNORM`, `D32_SFLOAT`).
- Direct fragment shader input attachment sampling via `subpassLoad`.
- Zero render pass objects.
