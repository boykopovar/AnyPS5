# libSceAgcDriver

## add Functional

- [GPU-only memory in frame capture](../../../core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp) (libSceAgcDriver) - a registered range that the GPU can access but the CPU cannot read (protection `0x10`, `0x20` or `0x30` without `0x1` or `0x2`) is not recorded, so a replay starts without what the title or the GPU stored there.
