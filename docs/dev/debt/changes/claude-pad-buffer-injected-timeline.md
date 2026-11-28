# libScePad

## add Functional

- [scePadRead with APS5_PAD_INPUT_FILE commands](../../../core/libs/prx/libScePad/src/PadState.cpp) (libScePad) - each command-file press or release since the previous `scePadRead` on port 0 is returned as its own sample at its change time, oldest dropped beyond the requested count, following the 64-entry change queue of [shadPS4's controller.cpp](https://github.com/shadps4-emu/shadPS4/blob/1d8004a22feb0d5cc58828bac066018e80b6d73f/src/input/controller.cpp#L61-L96). When physical changes are queued, both are returned in time order and changes beyond the requested count stay for the next read. Gaps: `APS5_PAD_SCRIPT` input (a buffered sample older than the last script change carries that change's time), IME or dialog capture and buttons held through a capture are applied as they are at the read, and script changes between reads are not buffered.
