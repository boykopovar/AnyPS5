# libSceMoveTracker

## change [libSceMoveTracker]

- [libSceMoveTracker](../../../core/libs/prx/libSceMoveTracker/Export.cpp) remains unimplemented apart from the answers before initialization in the entry below. Calibration direction, camera/controller input, capture initialization, state query, memory-size query and playback output signatures are recovered. Camera and controller records occupy 24 bytes, state records 160 bytes, and playback metadata 16 bytes. sceMoveTrackerInit validates work-memory, a nonzero 64-bit argument and GPU queue selectors below eight, returning 0x80ef0003; internal initialization and playback helper signatures remain unverified.

## add Unknown function info

- [sceMoveTrackerCalibrateReset, sceMoveTrackerCalibrateYaw, sceMoveTrackerCameraUpdate, sceMoveTrackerControllersUpdate, sceMoveTrackerGetState and sceMoveTrackerTerm before initialization](../../../core/libs/prx/libSceMoveTracker/Export.cpp) (libSceMoveTracker) - only the answer before `sceMoveTrackerInit` is implemented: they return `0x80ef0001` whatever their arguments, as firmware 13.42's `libSceMoveTracker.sprx` does while its byte `0xb1908` (zero-fill memory, set only by `sceMoveTrackerInit` at `0x5a8`, cleared by `sceMoveTrackerTerm` at `0x1bb1`) is 0; read from its disassembly and run under an emulator, not measured on a console. `sceMoveTrackerInit` throws past its argument checks here, so they always give that answer; they must follow the byte once it is implemented. The name of `0x80ef0001` is unknown
