# libkernel

## add Functional

- [sceKernelPollEventFlag and sceKernelWaitEventFlag](../../../core/libs/prx/libkernel/EventFlag/src/EventFlag.cpp) (libkernel) - copying `result_pat` and `timeout` through `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` and returning `SCE_KERNEL_ERROR_EFAULT` while preserving `errno` on invalid pointers is a host-safety fallback; console behaviour on unmapped pointers is unmeasured.
