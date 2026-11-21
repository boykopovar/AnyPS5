# libkernel

## add Functional

- [sceKernelLoadStartModule](../../../core/libs/prx/libkernel/Module/src/Module.cpp) (libkernel) - the module path is copied in with `CopyGuestPath_nid_no_patch` (1 KiB) before it is resolved, and an unreadable or unterminated path returns `SCE_KERNEL_ERROR_EFAULT`; the console's behaviour on such a pointer is unmeasured, so the host-safety answer is chosen instead of a fault, and no `dlerror` message is recorded for it.
