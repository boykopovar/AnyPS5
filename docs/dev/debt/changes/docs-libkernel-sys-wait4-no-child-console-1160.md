# libkernel_sys

## change [wait3 and wait4]

- [wait3 and wait4](../../../core/libs/prx/libkernel_sys/Wait.cpp) (libkernel_sys) - On Windows a reaped child's usage holds only `ru_utime` and `ru_stime`, from [`GetProcessTimes`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes) on its handle; the other fields, `ru_maxrss` included, are 0 as in Windows `getrusage`, and unlike FreeBSD 11.4 ([`exit1`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_exit.c)) the times omit descendants the child reaped. Console resource usage is unverified. Without a child process, a payload process on PS5 firmware 11.60 got -1 with `ECHILD` from `wait4` for pids -1, 1 and 0x7fffffff with `WNOHANG`, with a null status pointer, with option bit 0x80000000 (inside the accepted mask here too) and blocking with options 0, and from `wait3`; the status word and a 144-byte rusage buffer stayed unchanged. With a child process, and in a title process, it was not measured.

## change [_wait4]

- [_wait4](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) (libkernel_sys) is measured on the console only without a child process: `_wait4(-1, &status, WNOHANG, NULL)` returned -1 with `ECHILD` and left the status word unchanged in a payload process on PS5 firmware 11.60, as `wait4` did. Otherwise it is unverified. It follows FreeBSD 11.4, where [`_wait4`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/sys/wait.2) is the wait4 system call under its underscore name, and forwards to libkernel_sys wait4, inheriting its limits (on Windows the resource usage holds only the user and system times).
