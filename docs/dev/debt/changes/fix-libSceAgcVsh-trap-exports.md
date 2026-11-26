# libSceAgcVsh

## change [sceAgcSuspendPointAndCheckStatus]

- [sceAgcSuspendPointAndCheckStatus](../../../core/libs/prx/libSceAgc/VshTraps.cpp) (libSceAgcVsh) - the firmware 4.03 `libSceAgcVsh.sprx` body is the module's reporter trap, so its Vsh export is now a trap in `libSceAgc/VshTraps.cpp` that throws `sceAgcSuspendPointAndCheckStatus() is not allowed to be called from agc vsh.` on every call. The `int 0x41` trap and the `line %d: ` prefix of the report, whose line numbers are baked into the firmware stub, are not reproduced, and the signature stays unverified because the stub ignores its arguments.

## change 23 more exports whose firmware Vsh bodies are the same reporter trap

- The other 23 firmware Vsh reporter traps are [trap exports](../../../core/libs/prx/libSceAgc/VshTraps.cpp) built from `libSceAgc/VshTraps.cpp` that throw `<name>() is not allowed to be called from agc vsh.` without writing a packet, instead of running the shared `libSceAgc` bodies that the Vsh build omits through `APS5_AGC_VSH`. The `int 0x41` trap and the report's `line %d: ` prefix are not reproduced, so a call throws instead of ending the process. `sceAgcCbReleaseMem`, whose Vsh body only traps on one path, still runs the shared body.
