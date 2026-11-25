# libkernel

## add Unknown function info

- [sceKernelWaitSema timeout](../../../core/libs/prx/libkernel/Semaphore/src/Semaphore.cpp) (libkernel) - a timeout pointer that cannot be read or written returns `SCE_KERNEL_ERROR_EFAULT` after the handle and `need` checks and before tokens are taken, as `sceKernelCancelSema` does for its output. That the console reports `EFAULT` here, and in this order, is not measured. If the pointer becomes unwritable during the wait, writing back the remaining time throws.
