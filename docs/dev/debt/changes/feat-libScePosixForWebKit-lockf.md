# libScePosixForWebKit

## add Unknown function info

- [lockf](../../../core/libs/prx/libScePosixForWebKit/RecordLock.cpp) (libScePosixForWebKit) - follows FreeBSD 11.4 [`lockf.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/lockf.c) at the pinned commit, with the `F_ULOCK`, `F_LOCK`, `F_TLOCK` and `F_TEST` values from FreeBSD [`unistd.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/unistd.h). It locks through this library's `fcntl` record locks from offset 0 for `size` bytes, from the current position. The test runs in one process, so the `F_TEST` result for a lock held by another process, and the blocking `F_LOCK` wait, are not exercised. The console's lock behaviour is not measured. The export is placed beside `fnmatch`; which console module imports it is not verified.
