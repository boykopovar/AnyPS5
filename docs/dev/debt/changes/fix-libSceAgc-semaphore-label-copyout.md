# libSceAgc

## add Functional

- [sceAgcGetSemaphoreLabel](../../../core/libs/prx/libSceAgc/Misc/src/Semaphore.cpp) (libSceAgc) - label destination pointer is validated upfront with GuestWritable_nid_no_patch and populated through GuestCopyOut_nid_no_patch preserving caller errno; throwing on unwritable pointer without altering internal state is host safety fallback with console hardware behavior unmeasured.
