# libkernel

## add Functional

- [open with O_CREAT and O_DIRECTORY](../../../core/libs/prx/libkernel/File/src/Open.cpp) (libkernel) - `O_RDONLY|O_CREAT|O_DIRECTORY` on a directory the caller may not read fails with `EACCES` on Linux and macOS hosts; [FreeBSD 11.4](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/vfs_vnops.c#L340-L351) keeps `O_CREAT` on this lookup, skips the access check and opens the directory. Not measured on a PS5.
