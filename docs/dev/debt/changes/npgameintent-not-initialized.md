# libSceNpGameIntent

## change [libSceNpGameIntent additional exports]

- [libSceNpGameIntent additional exports](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) - these name/NID pairs are present in the inspected export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`, except the answer before initialization described in the "sceNpGameIntent calls before initialization" entry.

## add Unknown function info

- [sceNpGameIntent calls before initialization](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) (libSceNpGameIntent) - `sceNpGameIntentGetPermittedIntentTypes`, `sceNpGameIntentLaunchAppForPinvoke`, `sceNpGameIntentRemoveLaunchCallback` and `sceNpGameIntentRequest` return `0x80553802` while `sceNpGameIntentInitialize` has not run (or `sceNpGameIntentTerminate` has), whatever their arguments, and throw otherwise: firmware 13.42's `libSceNpGameIntent.sprx` tests the byte `0xc089` first, which only `sceNpGameIntentInitialize` sets (`0x8b7`) and `sceNpGameIntentTerminate` clears (`0x8f1`); read from its disassembly and run under an emulator, not measured on a console. What they do after initialization, and their signatures, are unknown
