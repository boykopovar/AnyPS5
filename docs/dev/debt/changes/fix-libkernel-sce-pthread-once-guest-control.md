# libkernel

## add Functional

- [scePthreadOnce](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp) (libkernel) - a non-null control pointer whose state is unreadable or unwritable returns `SCE_KERNEL_ERROR_EFAULT` (`0x8002000e`) via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, without running the initializer; returning `SCE_KERNEL_ERROR_EFAULT` instead of faulting the host is a host-safety fallback and is not measured on a console.

