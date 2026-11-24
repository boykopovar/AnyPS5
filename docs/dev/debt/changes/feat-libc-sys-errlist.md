# libc

## add Unknown function info

- [sys_errlist and sys_nerr](../../../core/libs/prx/libc/src/Errors.cpp) (libc) - the table is the errno message table this library already used for `strerror`, and `sys_nerr` is its 97 entries. FreeBSD 11.4 [`errlst.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/errlst.c) has 97 entries and the same first and last messages, and a comparison of all 97 entries with it found one difference: entry 30 is `Read-only filesystem` here, where FreeBSD has `Read-only file system`. That text is kept because the measured `strerror` of the titles returns it. `strerror` returns the table text only for errno 1 to 94; for 95 and above it keeps its own extended messages, so `strerror` and `sys_errlist` differ for those values. The console's table is not measured. The symbols are placed in libc; which console module exports them is not verified.
