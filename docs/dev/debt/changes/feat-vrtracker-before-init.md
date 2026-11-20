# libSceVrTracker

## add Functional

- [sceVrTrackerGetTime, sceVrTrackerGpuSubmit and sceVrTrackerGpuWait](../../../core/libs/prx/libSceVrTracker/Export.cpp) (libSceVrTracker) - only the answer before initialization is implemented, and they throw once `sceVrTrackerInit` has succeeded. The `NOT_INIT` code (`0x81260801`) and that it comes before any argument check follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/8d50471b7dc141dd700b51276b8edd1f293bce26/src/core/libraries/vr_tracker/vr_tracker.cpp#L221-L255), a PS4 implementation, not a PS5 measurement. Closes when: measured on a PS5 and the initialized answers are implemented
