# libkernel

## add Functional

- [scePthreadOnce](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp) (libkernel) - a non-null control pointer whose state is unreadable or unwritable returns `SCE_KERNEL_ERROR_EFAULT` (`0x8002000e`) via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, without running the initializer; active initializers are tracked on the host so a failed final write of `Done` still wakes concurrent waiters and subsequent calls observing a stale `Running` state return `SCE_KERNEL_ERROR_EFAULT` instead of waiting indefinitely. Returning `SCE_KERNEL_ERROR_EFAULT` instead of faulting the host is a host-safety fallback and is not measured on a console.
