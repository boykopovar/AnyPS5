# libc

## add Unknown function info

- [getwd](../../../core/libs/prx/libc/src/Getwd.cpp) (libc) - follows the FreeBSD 11.4 [`compat-43/getwd.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/compat-43/getwd.c) at the pinned commit: `getcwd` into a 1024-byte buffer (`MAXPATHLEN`, from `PATH_MAX` 1024), and on failure the `strerror_r` text of the error is written into the buffer and null is returned. The failure path is not exercised by the test. A null buffer throws.
