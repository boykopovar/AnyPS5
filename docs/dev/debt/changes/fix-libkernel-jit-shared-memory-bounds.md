# libkernel

## change [sceKernelJitCreateSharedMemory, sceKernelJitCreateAliasOfSharedMemory, sceKernelJitMapSharedMemory]

- [sceKernelJitCreateSharedMemory, sceKernelJitCreateAliasOfSharedMemory, sceKernelJitMapSharedMemory](../../../core/libs/prx/libkernel/DirectMemory/Jit.cpp) (libkernel) validate `fdOut`/`startOut` upfront via `GuestWritable_nid_no_patch` (after `EINVAL`, `EBADF`/`EACCES`, and length `ENOMEM` bounds, and after `GuestCopyIn_nid_no_patch` on `startOut` in `sceKernelJitMapSharedMemory`) and via `GuestCopyOut_nid_no_patch`, returning `SCE_KERNEL_ERROR_EFAULT` on bad memory without opening/duplicating descriptors or mapping and preserving caller `errno` across checks and rollback; Windows `CreateBacking` also rejects page-aligned lengths above `INT64_MAX` with `SCE_KERNEL_ERROR_ENOMEM` (matching POSIX `off_t`) instead of throwing `std::system_error`. Host-safety fallbacks; console fault handling and length-bound behavior are unmeasured.
