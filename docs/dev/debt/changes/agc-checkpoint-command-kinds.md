# libSceAgcDriver

## add Functional

- [Command kind checkpoints](../../../core/libs/prx/libSceAgcDriver/Graphics/src/CommandCheckpoints.cpp) (libSceAgcDriver): with APS5_GPU_CHECKPOINTS and VK_NV_device_diagnostic_checkpoints, a checkpoint before each recorded command and after vkBeginCommandBuffer names only the command kind and the last guest token of its command buffer on the recording thread; a thread recording two command buffers at once, a replay on another thread, or a buffer begun again on another thread loses or keeps a stale token. vkCmdBind*, vkCmdPush* and vkCmdSet* other than events pass as state, and the source scan in the test sees only literal, PFN_ and X(Cmd...) names; submit semaphore waits get none (a stuck wait shows the last command buffer end); kind markers carry no submit serial. One driver per process: real functions are process-wide and a second vkCmdSetCheckpointNV throws. NV checkpoint data for a queue can be empty after a reset.
