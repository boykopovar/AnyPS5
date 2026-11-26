# libSceNpCommon

## change [sce::np::Semaphore]

- [sce::np::Semaphore](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - `Wait` and `Signal` halt through `sceNpPanic` when the semaphore is not initialized, where `libSceNpCommon.sprx` of firmware 13.60 asserts `IsInit()` and panics, read from disassembly, not measured on a console. `Wait` with a timeout of 0 waits without a timeout, as the module does.
