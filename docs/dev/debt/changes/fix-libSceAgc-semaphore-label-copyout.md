# libSceAgc

## add Functional

- [sceAgcGetSemaphoreLabel](../../../core/libs/prx/libSceAgc/Misc/src/Semaphore.cpp) (libSceAgc) - label destination pointer is validated upfront under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch` and populated through `GuestCopyOut_nid_no_patch` preserving caller errno; throwing on unwritable pointer without altering internal state is a host-safety fallback with console hardware behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
