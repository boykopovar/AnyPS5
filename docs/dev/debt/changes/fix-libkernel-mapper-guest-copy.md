# libkernel

## add Functional

- [sceKernelCallIndirectBuffer2 and sceKernelCallIndirectBuffer3](../../../core/libs/prx/libkernel/SysExports.Mapper.cpp) (libkernel and libkernel_sys) - returning `SCE_KERNEL_ERROR_EFAULT` when `result` or `id` is non-null and unwritable via `GuestWritable_nid_no_patch` / `GuestCopyOut_nid_no_patch` is a host-safety fallback; console behaviour on unmapped output pointers is unmeasured.
