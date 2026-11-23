# libScePosixForWebKit

## add Unknown function info

- [nftw](../../../core/libs/prx/libScePosixForWebKit/FileTree.cpp) (libScePosixForWebKit) - the type flags (FTW_F, FTW_D, FTW_DNR, FTW_DP, FTW_NS, FTW_SL, FTW_SLN), the `struct FTW` fields and the FTW_PHYS and FTW_DEPTH behaviour follow the FreeBSD 11.4 [`ftw.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/ftw.h) and nftw.c at the pinned commit. The walk uses this library's guest `opendir`, `readdir`, `stat` and `lstat`, not fts(3); a directory already on the walk path is skipped without a callback, as FreeBSD skips FTS_DC. Only one directory handle is open at a time, so `nopenfd` is checked for at least 1 and not otherwise used. FTW_CHDIR and FTW_MOUNT, and unknown flag bits, throw. The export is placed in libScePosixForWebKit beside `fnmatch` and `glob`; which console module imports it is not verified.
