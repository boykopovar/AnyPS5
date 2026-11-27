# libkernel

## add Functional

- [sceKernel file and descriptor errno](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - whether every `sceKernel*` POSIX file wrapper in `Stdio.cpp` and `Open.cpp` (`sceKernelOpen`, `Close`, `Read`, `Write`, `Lseek`, `Stat`, `Unlink`, `Fstat`, `Fsync`, `Fdatasync`, `Getdirentries`, `Getdents`, `Mkdir`, `Rmdir`, `Rename`, `Chmod`, `Fchmod`, `Truncate`, `Utimes`, `Futimes`, `Pread`, `Pwrite`, `Readv`, `Writev`, `Preadv`, `Pwritev`, `Rdup`) sets guest `errno` on every error path, including user-space argument checks and host-safety `EFAULT` copies, has not been individually measured on a console. All of them store the low 16 bits of a negative `0x8002xxxx` return in `*__error()` and preserve it on success, matching `sceKernelFcntl` on firmware 13.60 and `sceKernelFtruncate`. Closes when: measured on a PS5.
