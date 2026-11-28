# libScePad

## add Functional

- [scePadRead with APS5_PAD_INPUT_FILE timelines](../../../core/libs/prx/libScePad/src/PadState.cpp) (libScePad) - each press or release of an injected timeline or pulse that happened since the previous `scePadRead` on port 0 is returned as its own sample, timed at the change, before the current sample, and the oldest are dropped when the call asks for fewer; this follows the 64-entry change buffer of [shadPS4's controller.cpp](https://github.com/shadps4-emu/shadPS4/blob/1d8004a22feb0d5cc58828bac066018e80b6d73f/src/input/controller.cpp#L61-L96). Injected changes are buffered only while no physical controller change is queued; with queued physical changes each queued sample carries the injected state at the read, as before.
