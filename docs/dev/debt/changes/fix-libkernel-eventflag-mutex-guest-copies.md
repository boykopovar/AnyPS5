# libkernel

## add Functional

- [sceKernelPollEventFlag and sceKernelWaitEventFlag](../../../core/libs/prx/libkernel/EventFlag/src/EventFlag.cpp) (libkernel) - writing the result pattern and reading and writing the wait timeout through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch`, returning `SCE_KERNEL_ERROR_EFAULT` while preserving `errno` on an inaccessible pointer, is a host-safety fallback: the console's result for such a pointer is not measured. A satisfied poll or wait has already cleared its bits when the result pattern cannot be written, and a wait whose remaining time cannot be written reports the fault after the wait completes.
