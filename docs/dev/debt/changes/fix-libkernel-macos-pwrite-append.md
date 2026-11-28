# libkernel

## change [sceKernelPwrite, pwrite, sceKernelPwritev, pwritev]

- [sceKernelPwrite, pwrite, sceKernelPwritev, pwritev](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) and the [AIO write commands](../../../core/libs/prx/libkernel/Aio/src/Aio.cpp) (libkernel) on an `O_APPEND` regular file on macOS append with `writev` and seek back to the previous offset, because macOS `pwrite` and `pwritev` write at the given offset even under `O_APPEND` (measured on macOS 27.2.1), where FreeBSD appends and leaves the offset unchanged. The write and the seek back hold the descriptor's positioned-transfer lock, shared with its duplicates, which libkernel's `read`, `write`, `readv`, `writev` and `lseek` also take while the descriptor is in append mode. Offset changes made without that lock can still interleave: a libc stream that macOS hands to its BSD stdio, another process sharing the description, and a call already running when `fcntl` turns `O_APPEND` on
