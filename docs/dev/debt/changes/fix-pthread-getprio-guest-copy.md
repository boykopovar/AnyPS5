# libkernel

## add Functional

- [scePthreadGetprio](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp) (libkernel) writes the priority to the guest output with `GuestCopyOut_nid_no_patch` after the thread is found, so an output that cannot be written returns `SCE_KERNEL_ERROR_EFAULT`, as `scePthreadGetaffinity` and `scePthreadGetname` do. This is a host-safety fallback; the console's behaviour with a bad output pointer was not measured. A null output still returns `SCE_KERNEL_ERROR_EINVAL`.
