# Assignment 66 – Programmable Rasterization Order & Subpass Shading (`VK_EXT_rasterization_order_attachment_access`)

## Overview
Leverage rasterization order attachment access to safely perform in-order fragment shader read-modify-write operations on color and depth attachments within dynamic rendering passes without memory hazard pipeline barriers, enabling order-independent transparency (OIT) and programmable blending.

## Key Concepts
- `VK_EXT_rasterization_order_attachment_access` feature flags (`rasterizationOrderColorAttachmentAccess`, `rasterizationOrderDepthAttachmentAccess`).
- `VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT`.
- In-place fragment shader read-modify-write without subpass dependencies.
- Real-time Order-Independent Transparency (OIT) and custom additive/subtractive blending algorithms.

## Acceptance Criteria
- [x] Query and enable `VK_EXT_rasterization_order_attachment_access` physical device features.
- [x] Configure dynamic rendering pipeline blend states with rasterization order attachment access flags.
- [x] Implement a fragment shader reading the current color attachment value, applying custom nonlinear blending, and outputting the result.
- [x] Render overlapping semi-transparent primitives ensuring deterministic primitive ordering.
- [x] Verify correct visual blending without GPU race conditions or validation layer warnings.

## Directory Structure
- `src/main.cpp`: Rasterization order attachment access host application.
- `CMakeLists.txt`: Build target configuration.
