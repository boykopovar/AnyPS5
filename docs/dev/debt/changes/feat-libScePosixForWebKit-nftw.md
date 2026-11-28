# libScePosixForWebKit

## add Unknown function info

- [nftw](../../../core/libs/prx/libScePosixForWebKit/FileTree.cpp) (libScePosixForWebKit) - which console module imports it is unverified; unknown flag bits throw. Flags, types and `struct FTW` follow FreeBSD 11.4 [`ftw.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/ftw.h), nftw.c and fts.c, walked with this library's guest `opendir`, `readdir`, `stat` and `lstat` instead of fts(3), one directory handle at a time, so `nopenfd` is only checked for at least 1. FTW_CHDIR (only with FTW_PHYS, as in fts) enters and leaves directories by absolute guest path instead of `fchdir` and `..`; a directory that opens but cannot be entered, which includes any outside the starting working directory, reports its entries as FTW_NS and ends the walk with -1, as FTS_ERR does.
