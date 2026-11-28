# libkernel

## add Functional

- [Transfers of more than `0x7ffff000` bytes on regular files](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) go to the host in parts of at most 1 GiB on Linux and macOS, as one Linux call moves at most `0x7ffff000` bytes and macOS `read` and `write` take at most `INT_MAX`, so another transfer or seek on the same open file, or a write to the same file, can run between two parts, where FreeBSD 11.4's [`vn_io_fault`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_vnops.c#L1162-L1199) holds the file offset and a range lock for the whole transfer. Not measured on a console. Still one host call: positioned and AIO writes to an `O_APPEND` file on macOS, and writes to shared memory objects.
