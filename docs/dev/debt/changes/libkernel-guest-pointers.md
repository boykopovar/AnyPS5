# libkernel

## add Functional

- [clock_gettime, clock_getres and sceKernelFutimes](../../../core/libs/prx/libkernel/Time/Time.cpp) (libkernel) - probing timespec outputs via `GuestCopyOut_nid_no_patch` in `clock_gettime` and `clock_getres` returning `EFAULT` on unmapped memory, and reading local times in `sceKernelFutimes`, are host-safety fallbacks; console fault behaviour on unmapped memory has not been measured.
