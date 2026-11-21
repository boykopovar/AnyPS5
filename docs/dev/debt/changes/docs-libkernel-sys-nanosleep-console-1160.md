# libkernel

## change [Sleeps shorter than 50 us on Windows]

- [Sleeps shorter than 50 us on Windows](../../../core/libs/prx/libkernel/Time/TimedWait.cpp) (libkernel) - on a firmware 11.60 payload process `nanosleep`, `sceKernelNanosleep` and `usleep(1)` never woke early and overshot by ~100 us (1 ns and 1 us: median 107 to 110 us; 100 us: 206 to 210 us). Here it never returns early: the thread spins to the deadline, exact to under a microsecond but keeping a core busy. Once a thread has issued such sleeps back to back for 1 ms (each starting sooner after the previous one ended than its own length), the next ones block on its high-resolution timer, which frees the core and stretches each to the host timer period (about 0.5 ms on Windows 11, 1 ms or more without a high-resolution timer), until the thread stays awake for at least the length of a request. A polling loop so reacts up to one timer period late after 1 ms of finding no work. These sleeps are not ended by GPU or flip completions. Linux and macOS pass the request to the host `nanosleep`

# libkernel_sys

## change [__sys_nanosleep]

- [__sys_nanosleep](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) follows FreeBSD 11.4 libc's [`nanosleep.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/sys/nanosleep.c) as the raw system call underneath `nanosleep`, executing without cancellation checks through `sceKernelNanosleep` and mapping errors to errno and -1. It inherits libkernel's sleep limits: `rmtp` is never written, `EINTR` is not produced, seconds are capped at `INT32_MAX / 2`, [short sleeps can wake early](libkernel.md#functional), and on a PS5 payload process (firmware 11.60) a 10 ms sleep took about 10.1 ms with `rmtp` untouched, `nsec` 1000000000 and -1 returned `EINVAL`, `sec` -1 returned 0 at once and a null `rqtp` returned `EFAULT`, as here for the return values and errors; `EINTR` and the upper limit were not measured.
