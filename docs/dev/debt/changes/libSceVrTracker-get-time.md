# libSceVrTracker

## add Functional

- [sceVrTrackerGetTime](../../../core/libs/prx/libSceVrTracker/Export.cpp) (libSceVrTracker) - returns `NOT_INIT` before initialization, `ARGUMENT_INVALID` for a null or unwritable pointer, and otherwise writes `sceKernelGetProcessTime()` in microseconds, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/vr_tracker/vr_tracker.cpp) does on PS4. The tracker's real timebase and its relation to camera frame timestamps are not measured on a PS5.
