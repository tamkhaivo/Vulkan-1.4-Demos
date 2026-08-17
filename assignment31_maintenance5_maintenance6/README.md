# Assignment 31 – Vulkan 1.4 Core Maintenance 5 & Maintenance 6 (`VK_KHR_maintenance5` / `VK_KHR_maintenance6`)

## Overview
Utilize Vulkan 1.4 core Maintenance 5 and Maintenance 6 enhancements to simplify buffer copies, support dynamic index buffer bounds, and perform descriptorless memory queries.

## Key Concepts
- Dynamic index buffer binding with byte bounds via `vkCmdBindIndexBuffer2KHR`.
- Direct shader staging copies with `vkCmdCopyBuffer2` / `vkCmdCopyImage2`.
- Direct buffer device address push constant passing without descriptor set allocations.
- Bound memory status and sub-resource layout queries.

## Acceptance Criteria
- [x] Enable `maintenance5` and `maintenance6` features in `VkPhysicalDeviceVulkan14Features`.
- [x] Load and execute `vkCmdBindIndexBuffer2KHR` with explicit index buffer byte sizes and offsets.
- [x] Pass dynamic buffer device addresses directly via push constants to shaders.
- [x] Execute synchronized copy commands using extended structure chains (`VkCopyBufferInfo2`).
- [x] Validate rendering pipeline runs without validation warnings or buffer out-of-bounds access.

## Directory Structure
- `src/main.cpp`: Maintenance 5 & 6 host application.
- `shaders/`: Shaders utilizing BDA and dynamic buffer references.
- `CMakeLists.txt`: Build target configuration.
