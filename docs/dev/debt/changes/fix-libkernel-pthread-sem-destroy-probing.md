# libkernel

## add Functional

- [scePthreadSemDestroy](../../../core/libs/prx/libkernel/Pthread/src/Sem.cpp) (libkernel) - safely probes the guest semaphore handle pointer via `GuestCopyIn_nid_no_patch` and `GuestWritable_nid_no_patch` before mutating semaphore state and stores the tombstone marker via `GuestCopyOut_nid_no_patch`, returning `SCE_KERNEL_ERROR_EFAULT` (0x8002000e) on unmapped or unwritable memory and `SCE_KERNEL_ERROR_EINVAL` on a null semaphore pointer while preserving caller `errno`. Returning `EFAULT` on inaccessible guest memory and `EINVAL` on a null semaphore pointer are host-safety fallbacks; retail console fault behavior has not been measured.
