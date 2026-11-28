# libkernel

## add Functional

- [sceKernelGetModuleInfoForUnwind output](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - `info` is read (`st_size`) through `GuestCopyIn_nid_no_patch` and preflighted and written (filled result or zeroed on invalid flags / `ESRCH`) through `GuestArena::HostWrite`, `GuestWritable_nid_no_patch` and `GuestCopyOut_nid_no_patch`, preserving caller `errno` and leaving unwritable page-straddling buffers unmodified. A non-null `info` that cannot be read or written returns `SCE_KERNEL_ERROR_EFAULT` (0x8002000e) as a host-safety fallback where the console's fault behaviour is unmeasured; null `info` with flags other than 1 or 2 still throws and null `info` with flags 1 or 2 still returns `SCE_KERNEL_ERROR_EFAULT`.
