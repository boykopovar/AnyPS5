# libkernel

## add Functional

- [sceKernelStopUnloadModule](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - returning EFAULT when result pointer is unwritable via GuestCopyOut is a host-safety fallback with console behavior unmeasured.
