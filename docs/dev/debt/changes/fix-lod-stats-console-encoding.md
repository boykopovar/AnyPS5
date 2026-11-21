# libSceAgc

## add Functional

- [sceAgcDcbGetLodStats](../../../core/libs/prx/libSceAgc/DcbDraw/src/Stats.cpp) (libSceAgc) - the packet is the console's opcode 0x8e. The [driver](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) validates it and skips it: no LOD statistics are collected and the buffer is never written, as with the NOP this replaced. `reportAndReset` and `reportingIntervalIn100kClocks` are stack arguments that prosper's measurement never set: values outside their ranges throw, and the replay's control word differs from the console's by the interval bit (0x4). The buffer address is masked with `~0x3f`; the measurement clears address bits 1, 2 and 5 only.
