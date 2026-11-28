# libkernel_sys

## change [openat, _openat]

- [openat, _openat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) pass `AT_FDCWD` (-100) and absolute paths to `open`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. They return `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. As with `open`, `O_NONBLOCK` is ignored. PPSA99003 opens directory entries through `openat`. On a PS5 payload process (firmware 11.60) `openat` opened an absolute path although the descriptor was not open, returned `EBADF` for a relative path with descriptor -1 or 999 and `ENOTDIR` for a file descriptor, answered `ENOENT` for `SUB` where the directory is `sub`, and resolved `..` and `../..` to the parent folders; `_openat` and a title process were not measured.

## change [mkdirat]

- [mkdirat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `mkdir`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. On Windows, `mode` is ignored. On a PS5 payload process (firmware 11.60) `mkdirat` through a directory descriptor created a directory, returned `EEXIST` for an existing name and `ENOENT` for a missing parent; invalid descriptors and a title process were not measured.

## change [readlinkat]

- [readlinkat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `readlink`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. `libkernel.sprx` on a PS5 payload process (firmware 11.60) does not export `readlinkat`, so it could not be measured there; this library exports it.

## change [unlinkat]

- [unlinkat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `rmdir` (with `AT_REMOVEDIR`, 0x800) or `unlink`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. Removing a directory without `AT_REMOVEDIR` fails with `EPERM`, removing `.` or `..` with `AT_REMOVEDIR` fails with `EINVAL`, and flags other than `AT_REMOVEDIR` fail with `EINVAL`. On a PS5 payload process (firmware 11.60) a directory without `AT_REMOVEDIR` gave `EPERM`, a directory that is not empty gave `ENOTEMPTY`, `.` and `..` with `AT_REMOVEDIR` and the flag 0x1 gave `EINVAL`; invalid descriptors and a title process were not measured.

## change [renameat]

- [renameat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `rename`. Any other relative path is joined to its descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to relative descriptors, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. On a PS5 payload process (firmware 11.60) `renameat` within a directory descriptor and into a second one returned 0 and the file was there afterwards, a missing source gave `ENOENT` and descriptor 999 gave `EBADF`; a title process was not measured.
