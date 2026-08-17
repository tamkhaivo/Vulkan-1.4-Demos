# Assignment 53 – Ray Tracing Position Fetch & BVH Geometry Extraction (`VK_KHR_ray_tracing_position_fetch`)

## Overview
Extract exact vertex positions and triangle index data directly from hardware BVH acceleration structures inside Closest-Hit and Any-Hit shaders without binding separate vertex/index storage buffers (`OpRayQueryGetIntersectionTriangleVertexPositionsKHR` / `VK_KHR_ray_tracing_position_fetch`).

## Key Concepts
- `VK_KHR_ray_tracing_position_fetch` feature flag (`rayTracingPositionFetch`).
- Building BLAS with `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR`.
- Fetching 3D object/world vertex positions directly from BVH leaves.
- On-the-fly barycentric normal interpolation and UV mapping without descriptor vertex bindings.

## Acceptance Criteria
- [x] Query and enable `rayTracingPositionFetch` physical device feature.
- [x] Build BLAS with `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR`.
- [x] Fetch triangle vertex positions directly in Closest-Hit shader via `hitTriangleVertexPositionsKHR`.
- [x] Calculate dynamic surface normals and barycentric weights.
- [x] Validate zero vertex buffer descriptor allocations and clean execution.

## Directory Structure
- `src/main.cpp`: Ray tracing position fetch configuration and execution host application.
- `CMakeLists.txt`: Build target configuration.
