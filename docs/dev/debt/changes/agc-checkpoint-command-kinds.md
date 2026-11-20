# libSceAgcDriver

## change [Device-lost fault attribution]

- [Device-lost fault attribution](../../../core/libs/prx/libSceAgcDriver/Execution/include/DeviceFault.hpp) (libSceAgcDriver): a device loss prints the faulting VA with its guest snapshot (owner or neighbours), a bracket window (last completed, in flight, first not started, last 8 tokens) and draw/dispatch/mesh tokens (serial, program, variant, stage, desc). Indirect draws/dispatches record buffer VA, offset, stride and up to 4 record-time words (unsynced read, never submits); mesh sets the stage. Verified-copy Find; clear-first Claim (reuse-tested). Residency today: never bound (faulting submit snapshot found and uncovered) else unknown; evicted/import/refused lines have no producer yet. Serial at marker offset 0, tokens at 4. Needs APS5_GPU_CHECKPOINTS. Cannot name T# or pending write-back; no task token path yet. Vendor dump via APS5_DUMP_DEVICE_FAULT where supported. Opened for PPSA01342 (#10634): second-world load faults a write.

## add Functional

- [Command kind checkpoints](../../../core/libs/prx/libSceAgcDriver/Graphics/src/CommandCheckpoints.cpp) (libSceAgcDriver): with APS5_GPU_CHECKPOINTS and VK_NV_device_diagnostic_checkpoints, a checkpoint before each recorded command and after vkBeginCommandBuffer names only the command kind and the last guest token of its command buffer on the recording thread; a thread recording two command buffers at once, or a replay on another thread, loses that token. NV checkpoint data for the main queue was empty after most resets seen on an RTX 5070 Ti (621.24), so a hang may name no main-queue command.
