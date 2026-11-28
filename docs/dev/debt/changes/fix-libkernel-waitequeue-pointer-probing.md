# libkernel

## change [sceKernelWaitEqueue](../../../core/libs/prx/libkernel/Equeue/Equeue.cpp) (libkernel) - firmware 13.42 checks only that the event count is positive

- [sceKernelWaitEqueue](../../../core/libs/prx/libkernel/Equeue/Equeue.cpp) (libkernel) - firmware 13.42 checks only that the event count is positive and then stores to the result pointer directly (read from disassembly, not measured on a console), so the console faults on a null result pointer; AnyPS5 returns `SCE_KERNEL_ERROR_EINVAL` instead. A null event list returns `SCE_KERNEL_ERROR_EFAULT` here before waiting, where the firmware passes it on to its wait call, whose answer is not measured. Unreadable timeout, unwritable result count, and unwritable event list outputs are probed via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` and return `SCE_KERNEL_ERROR_EFAULT` (0x8002000e) as host-safety fallbacks.
