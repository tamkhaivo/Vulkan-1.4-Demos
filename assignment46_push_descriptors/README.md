# Assignment 46 – Zero-Allocation Push Descriptors (`VK_KHR_push_descriptor` / Vulkan 1.4 Core)

## Overview
Eliminate `VkDescriptorPool` and `VkDescriptorSet` allocation overhead for frequently changing uniform buffers, storage buffers, and sampled images by recording descriptor updates directly into the `VkCommandBuffer` stream with `vkCmdPushDescriptorSetKHR`.

## Key Concepts
- Vulkan 1.4 core Push Descriptors (`VK_KHR_push_descriptor`, `VkPhysicalDevicePushDescriptorPropertiesKHR`).
- Creating `VkDescriptorSetLayout` with `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR`.
- Pushing dynamic UBO and SSBO descriptor writes directly with `vkCmdPushDescriptorSetKHR`.
- Descriptor update templates (`VkDescriptorUpdateTemplate`) optimized for push descriptors (`VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR`).
- Eliminating host descriptor pool locking, allocation churn, and frame memory fragmentation.

## Acceptance Criteria
- [x] Query and verify `maxPushDescriptors` support via `VkPhysicalDevicePushDescriptorPropertiesKHR`.
- [x] Create a `VkDescriptorSetLayout` marked with `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR`.
- [x] Push dynamic uniform and storage buffer descriptor writes directly into the command buffer stream using `vkCmdPushDescriptorSetKHR`.
- [x] Update push descriptors using `vkCmdPushDescriptorSetWithTemplateKHR` with `VkDescriptorUpdateTemplate`.
- [x] Validate zero CPU descriptor pool allocations and hitch-free command submission.

## Directory Structure
- `src/main.cpp`: Push descriptor layout setup, command buffer recording, and execution host application.
- `CMakeLists.txt`: Build target configuration.
