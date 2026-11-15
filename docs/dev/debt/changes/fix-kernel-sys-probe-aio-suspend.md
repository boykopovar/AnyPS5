# libkernel_sys

## add Unknown function info

- [aio_suspend inputs](../../../core/libs/prx/libkernel_sys/Aio.cpp) (libkernel_sys) - the control-block list and the timeout are copied in with `GuestCopyIn_nid_no_patch` before waiting (as FreeBSD's `copyin` does), and an unreadable one fails with `EFAULT` like the null list and `aio_waitcomplete`'s unreadable timeout; the console's behaviour on unmapped memory is unmeasured.
