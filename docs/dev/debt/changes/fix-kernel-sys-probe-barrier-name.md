# libkernel_sys

## add Unknown function info

- [scePthreadBarrierInit name](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - the name is copied with `CopyGuestString_nid_no_patch` capped at 32 bytes, which keeps the `ENAMETOOLONG` result for 32 or more characters; an unreadable name returns `SCE_KERNEL_ERROR_EFAULT` without creating the barrier, because the console's behaviour on unmapped memory is unmeasured.
