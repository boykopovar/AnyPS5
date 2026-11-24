# libScePosixForWebKit

## add Unknown function info

- [ftw](../../../core/libs/prx/libScePosixForWebKit/FileWalk.cpp) (libScePosixForWebKit) - follows the FreeBSD 11.4 [`ftw.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/ftw.c) at the pinned commit: logical walk, preorder only, `FTW_F`, `FTW_D`, `FTW_DNR` and `FTW_NS` (a dangling link or missing entry is `FTW_NS`), a directory already on the walk path ends the walk with -1 and `ELOOP`, as FreeBSD `ftw.c` does for a directory cycle, and a callback result other than zero stops the walk and is returned. The walk uses this library's guest `opendir`, `readdir` and `stat`, not fts(3). Only one directory handle is open at a time, so `nfds` is checked for at least 1 and is otherwise not used. The export is placed beside `fnmatch`; which console module imports it is not verified.
