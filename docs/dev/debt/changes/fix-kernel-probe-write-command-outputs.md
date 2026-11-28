# libkernel

## add Functional

- [sceKernelWriteModifyMtypeProtectCommand and sceKernelWriteRemapIntoPrtCommand guest outputs](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - the command and its size are written with GuestCopyOut_nid_no_patch in the same order as before; returning `SCE_KERNEL_ERROR_EFAULT` for a non-null unwritable `dst` or `outSize` is a host-safety fallback (the null outputs stay optional, and the range and protection checks still return `SCE_KERNEL_ERROR_EINVAL` first), and whether the console faults or returns an argument error on unmapped memory is unmeasured.
