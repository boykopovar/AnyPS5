# libScePad

## add Functional

- [scePadRead with APS5_PAD_INPUT_FILE commands](../../../core/libs/prx/libScePad/src/PadState.cpp) (libScePad) - each command-file press or release since the previous `scePadRead` on port 0 is returned as its own sample at its change time, oldest dropped beyond the requested count, following the 64-entry change queue of [shadPS4's controller.cpp](https://github.com/shadps4-emu/shadPS4/blob/1d8004a22feb0d5cc58828bac066018e80b6d73f/src/input/controller.cpp#L61-L96). Gaps: these samples are buffered only while no physical change is queued (otherwise every queued sample carries the injected state at the read); `APS5_PAD_SCRIPT` input, IME or dialog capture and buttons held through a capture are applied as they are at the read, and script changes between reads are not buffered. The console's sampling cadence is not measured.
