# libkernel

## add Functional

- [sceKernelAprResolveFilepathsToIds](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - returning EFAULT when error_index is unwritable via GuestCopyOut is a host-safety fallback with console behavior unmeasured.
