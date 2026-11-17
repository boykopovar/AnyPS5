# libkernel_sys

## add Functional

- [sceKernelCallIndirectBuffer2, sceKernelCallIndirectBuffer3](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - returning EFAULT when output pointers are unwritable via GuestCopyOut is a host-safety fallback with console behavior unmeasured.
