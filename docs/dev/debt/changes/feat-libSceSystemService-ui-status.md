# libSceSystemService

## add Silent stubs

- [sceShellCoreUtilGetUIStatus and sceShellCoreUtilSetUIStatus](../../../core/libs/prx/libSceSystemService/UiStatus.cpp) (libSceSystemService) - the 64-bit status word is host-local state that starts at zero and is read back through a guarded copy, and `sceSystemServiceGetUIStatus` reads the same word. `sceShellCoreUtilSetUIStatus(status, mask)` replaces the bits `mask` selects and leaves the others alone, inferred from the two arguments rather than measured on a console. AnyPS5 draws no shell UI over the title, so the word changes nothing else. A null output returns `0x80A10003` and an unwritable one returns the same; that result is a host-safety choice and is unmeasured.
