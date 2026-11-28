# libkernel

## add Functional

- [scePthreadSemPost, scePthreadSemTrywait, scePthreadSemWait, and scePthreadSemTimedwait](../../../core/libs/prx/libkernel/Pthread/src/Sem.cpp) (libkernel) - read the guest `PthreadSem` handle through `GuestCopyIn_nid_no_patch` and return `SCE_KERNEL_ERROR_EFAULT` (0x8002000e) on non-null unreadable pointers while preserving caller `errno`, instead of dereferencing `*sem` raw and faulting the host. `sem == nullptr` still throws `std::invalid_argument`, uninitialized, destroyed, or unknown handles still throw `std::runtime_error`, and `scePthreadSemInit` and `scePthreadSemGetvalue` also preserve `errno` across guest copies. Returning `EFAULT` on unreadable semaphore pointers is a host-safety fallback; retail console fault behavior is unmeasured.
