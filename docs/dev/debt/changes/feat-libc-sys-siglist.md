# libc

## add Unknown function info

- [sys_siglist and sys_nsig](../../../core/libs/prx/libc/src/SignalNames.cpp) (libc) - follow FreeBSD 11.4 [`siglist.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/siglist.c) at the pinned commit: 32 entries, from `Signal 0` to `User defined signal 2`, and `sys_nsig` is their count, as FreeBSD computes it from the array size. `NSIG` is 32 in FreeBSD [`signal.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/sys/signal.h). The console's table is not measured. `sys_signame` and `strsignal` are not implemented here. The symbols are placed in libc; which console module exports them is not verified.
