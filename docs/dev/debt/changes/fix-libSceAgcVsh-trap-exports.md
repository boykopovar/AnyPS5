# libSceAgcVsh

## change [sceAgcSuspendPointAndCheckStatus]

- [sceAgcSuspendPointAndCheckStatus](../../../core/libs/prx/libSceAgcVsh/Export.cpp) (libSceAgcVsh) - the firmware 4.03 `libSceAgcVsh.sprx` body is the module's reporter trap, so its Vsh export is now a trap in [libSceAgc/VshTraps.cpp](../../../core/libs/prx/libSceAgc/VshTraps.cpp), compiled only into libSceAgcVsh; the shared libSceAgc build still returns the bound suspend-point count.

## change 23 more exports whose firmware Vsh bodies are the same reporter trap

- The other 23 firmware Vsh reporter traps listed in [libSceAgcVsh/CMakeLists.txt](../../../core/libs/prx/libSceAgcVsh/CMakeLists.txt) are trap exports in [libSceAgc/VshTraps.cpp](../../../core/libs/prx/libSceAgc/VshTraps.cpp) and now throw `std::runtime_error("<name>() is not allowed to be called from agc vsh.")`, which the Vsh build omits through `APS5_AGC_VSH`. The `int 0x41` trap and the report's `line %d: ` prefix are not reproduced, so a call throws instead of ending the process. `sceAgcCbReleaseMem`, whose Vsh body only traps on one path, still runs the shared body.
