# libkernel

## add Functional

- [sceKernelPollEventFlag, sceKernelWaitEventFlag](../../../core/libs/prx/libkernel/EventFlag/src/EventFlag.cpp) (libkernel) - returning EFAULT when result pattern pointer is unwritable via GuestCopyOut is a host-safety fallback with console behavior unmeasured.
