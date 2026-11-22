# libSceAgcDriver

## add Unknown function info

- [PS4 (GCN) linear-general color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not checked on a console. A color target with `CB_COLOR_INFO.LINEAR_GENERAL` set ignores its tile mode index and is linear with the pitch `CB_COLOR_PITCH` gives, unpadded, at `CB_COLOR_BASE` plus `CB_COLOR_VIEW.SLICE_START[7:0]` bytes, with no slice offset, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/57e129ec548cfcf8572c0a26b1c9aaf7e95dc3b6/src/video_core/amdgpu/regs_color.h) reads these registers. `SLICE_START[10:8]` set, and multisampled linear-general targets, throw; endian-swapped targets still throw.
