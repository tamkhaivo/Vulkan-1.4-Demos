# Assignment 74 – Device Address Binding Reporting & Post-Mortem GPU Page-Fault Telemetry (VK_EXT_device_address_binding_report / VK_EXT_device_fault)

## Overview & Architectural Critique
Buffer Device Address (BDA) dereferencing errors cause instantaneous GPU hangs and Driver TDR crashes without actionable CPU call stacks.

**Assignment 74** implements a **BDA Address Telemetry & Page-Fault Isolation Engine**:
1. **Device Address Binding Reporting (`VK_EXT_device_address_binding_report`)**: Tracks GPU virtual address ranges directly via Vulkan debug utility messengers as buffers are allocated and bound.
2. **GPU Fault Registry (`VK_EXT_device_fault`)**: Queries hardware fault registers and fault addresses upon encountering `VK_ERROR_DEVICE_LOST`.
3. **Interval Tree Pointer Resolver**: Matches any offending 64-bit GPU virtual address directly back to its source buffer and byte offset.

## Key Vulkan 1.4 Concepts
- **`VK_EXT_device_address_binding_report`**: Real-time driver callbacks on BDA buffer creation and lifetime changes.
- **`VK_EXT_device_fault`**: Querying `VkDeviceFaultCountsEXT` and `VkDeviceFaultAddressInfoEXT`.
- **Dynamic Rendering & BDA Access**: Rendering textured/colored scene geometry using raw 64-bit GPU memory pointers.

## Acceptance Criteria
- [x] Enable `VK_EXT_device_address_binding_report` and register debug messenger.
- [x] Track GPU buffer device addresses in a CPU-side registry.
- [x] Render scene with BDA buffer references.
- [x] Clean execution with 0 Vulkan validation errors.

## Directory Structure
- `src/main.cpp`: Host application with BDA address registry and fault telemetry hooks.
- `shaders/bda_fault.vert`, `shaders/bda_fault.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
