# libScePosixForWebKit

## add Unknown function info

- [ualarm](../../../core/libs/prx/libScePosixForWebKit/Alarm.cpp) (libScePosixForWebKit) - follows FreeBSD 11.4 [`ualarm.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/ualarm.c) at the pinned commit: it arms `ITIMER_REAL` through `setitimer` and returns the microseconds that the previous timer had left, or `(useconds_t)-1` on failure. Delivery of `SIGALRM` to a guest handler is not implemented by `setitimer` (see the existing `ITIMER_REAL` entries in [libkernel](libkernel.md)), so an expiring `ualarm` timer that has a guest handler throws. The console's behaviour is not measured. The export is placed beside `fnmatch`; which console module imports it is not verified.
