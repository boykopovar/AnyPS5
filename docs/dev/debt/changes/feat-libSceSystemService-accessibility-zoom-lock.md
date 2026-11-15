# libSceSystemService

## add Unknown function info

- [sceShellCoreUtilAccessibilityZoomLock, sceShellCoreUtilAccessibilityZoomUnlock, sceShellCoreUtilIsAccessibilityZoomLocked](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - a process-wide flag that Lock sets and Unlock clears; the getter reports it for the running title and throws for another app ID. AnyPS5 has no system zoom overlay for the lock to suppress. The `int (void)` signatures and the NIDs `5SfMtsW8h7A` and `Uku2JpZmoqc` follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/d4ea43b9ff7369365a2dcf9b15315c65aeb7f6f9/src/core/libraries/system/systemservice.cpp#L575-L583), which stubs both without arguments; shadPS4 also declares the getter without arguments, while its app ID here is the one the pending exports recovered. Whether the console reports a lock without a lock call, and the result of locking twice, are unmeasured: the flag starts false and repeated calls keep the last state.
