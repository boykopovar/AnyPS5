# libkernel

## add Functional

- [Transfers of more than `0x7ffff000` bytes on Windows regular files](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) go to the host in parts of at most 1 GiB, as [`_read`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/read) takes at most `INT_MAX` bytes and `_write` returns an `int`, so a write to the same file through another open file can run between two parts, where FreeBSD 11.4's [`vn_io_fault`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_vnops.c#L1162-L1199) holds a range lock for the whole transfer. A part that fails after earlier parts moved data returns the bytes moved, as on Linux and macOS, where FreeBSD returns the error. Not measured on a console.
- [read and write of more than `INT_MAX` bytes on a Windows pipe or character device](../../../core/libs/prx/libkernel/File/src/Open.cpp) (libkernel) still go to one `_read` or `_write` call, which cannot move or report more than `INT_MAX` bytes, and throw above `UINT_MAX` bytes. FreeBSD 11.4 accepts up to `SSIZE_MAX` bytes in one call.
