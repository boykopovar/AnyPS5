# libkernel

## add Functional

- [scePthreadAttrGet, scePthreadAttrGetschedparam, and scePthreadAttrSetstackaddr](../../../core/libs/prx/libkernel/Pthread/src/Attr.cpp) (libkernel) - loading `attr` and storing `KernelSchedParam::sched_priority` via `LoadAttribute` / `StoreOutput` (`GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`) and returning `SCE_KERNEL_ERROR_EFAULT` on unmapped pointers is a host-safety fallback; console behaviour on invalid pointers is unmeasured.
