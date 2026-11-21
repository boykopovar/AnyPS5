# libkernel

## add Functional

- [scePthreadRwlockInit guest output](../../../core/libs/prx/libkernel/Pthread/src/Rwlock.cpp) (libkernel) - the new lock handle is written with GuestCopyOut_nid_no_patch; returning `SCE_KERNEL_ERROR_EFAULT` for a non-null unwritable `rwlock`, as `scePthreadRwlockattrInit` and `scePthreadRwlockattrDestroy` in the same file do for their handle, is a host-safety fallback (the new lock is freed first, and a null `rwlock` still throws), and whether the console faults or returns an error on unmapped memory is unmeasured.
