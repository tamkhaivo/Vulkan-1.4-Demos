# Assignment 9 – Multi-Threaded Command Recording with Timeline Semaphores

## Overview
Record secondary command buffers in parallel across worker CPU threads and submit via primary command buffers synchronized with timeline semaphores.

## Key Concepts
- Thread-local `VkCommandPool` allocation.
- Secondary command buffers (`vkCmdExecuteCommands`).
- Timeline semaphores (`VkSemaphoreTypeCreateInfo`, monotonically increasing values, CPU host signal/wait).
