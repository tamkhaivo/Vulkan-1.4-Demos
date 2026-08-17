# Assignment 31 – Vulkan 1.4 Maintenance 5 & Maintenance 6 (`VK_KHR_maintenance5` / `VK_KHR_maintenance6`)

## Overview & Architectural Critique
Vulkan 1.4 core incorporates the **Maintenance 5 (`VK_KHR_maintenance5`)** and **Maintenance 6 (`VK_KHR_maintenance6`)** specifications, resolving numerous edge-case inefficiencies in index buffers, shader staging copies, push constants, and bound memory status queries.

Notable enhancements include `vkCmdBindIndexBuffer2KHR` (allowing custom size bounds and non-zero first indices directly in command buffers), `VkPhysicalDeviceMaintenance5PropertiesKHR` providing direct copy formats without VkBuffer allocations, and dynamic push constants per shader stage with fine-grained binding ranges.

## Key Vulkan 1.4 Concepts
- **Dynamic Index Range Binding**: `vkCmdBindIndexBuffer2KHR` taking `VkDeviceSize size` and `VkIndexType indexType`.
- **Bound Device Memory Status**: `vkGetDeviceImageMemoryRequirementsKHR` and `vkGetDeviceBufferMemoryRequirementsKHR` querying memory requirements without creating dummy objects.
- **Shader Staging Copies**: Direct transfer copies between shader storage memory and image subsystems.

## Concrete Implementation Example (Vulkan 1.4 C++)

```cpp
// 1. Dynamic Index Buffer with Maintenance 5 vkCmdBindIndexBuffer2KHR
VkDeviceSize submeshIndexOffset = currentSubmesh * sizeof(uint32_t) * indicesPerSubmesh;
VkDeviceSize submeshIndexSize = sizeof(uint32_t) * indicesPerSubmesh;

vkCmdBindIndexBuffer2KHR(
    cmd,
    unifiedIndexBuffer,
    submeshIndexOffset,
    submeshIndexSize,        // Bounds checked directly by hardware/validation
    VK_INDEX_TYPE_UINT32
);

// 2. Query Device Buffer Memory Requirements without VkBuffer Handle (Maintenance 5)
VkDeviceBufferMemoryRequirementsKHR bufferReqsInfo{
    .sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR,
    .pCreateInfo = &bufferCreateInfo
};
VkMemoryRequirements2 memReqs2{
    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
};
vkGetDeviceBufferMemoryRequirementsKHR(device, &bufferReqsInfo, &memReqs2);

// 3. Render submesh bounded safely
vkCmdDrawIndexed(cmd, indicesPerSubmesh, 1, 0, 0, 0);
```

## Acceptance Criteria
- [x] Query and enable `VK_KHR_maintenance5` and `VK_KHR_maintenance6` features in Vulkan 1.4.
- [x] Utilize `vkGetDeviceBufferMemoryRequirementsKHR` to query allocation requirements directly from create info.
- [x] Record draw commands utilizing `vkCmdBindIndexBuffer2KHR` with dynamic size bounds.
- [x] Validate zero memory overrun warnings and clean validation layer output.

## Directory Structure
- `src/main.cpp`: Maintenance 5 & 6 host application.
- `shaders/maint_scene.vert`, `shaders/maint_scene.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
