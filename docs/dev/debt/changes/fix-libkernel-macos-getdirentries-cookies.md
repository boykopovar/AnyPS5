# libkernel

## change [macOS directory entries]

- [macOS directory entries](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - `sceKernelGetdirentries` reads on from the host position of `__getdirentries64`, which gives no cookie per entry, so when the file system refuses a read of just the entries that fit the guest buffer (below 48 bytes, or a few hundred on exFAT), the rest are kept for the descriptor and its duplicates: those calls repeat `*basep`, a `seekdir` inside them lands on the wrong entry, and an `lseek` drops them. Measured on macOS 27.2.1: an entry removed between calls skips none on APFS, FAT and exFAT but skips on HFS+, whose cookies are indexes; FAT and exFAT set the offset to -1 at the end, where `lseek(fd, 0, SEEK_CUR)` fails with `EINVAL` and `*basep` is -1. Two threads reading one descriptor can interleave
