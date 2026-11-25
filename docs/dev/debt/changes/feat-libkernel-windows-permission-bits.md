# libkernel

## add Functional

- [Permission bits on Windows](../../../core/libs/prx/libkernel/File/include/WindowsPermissions.hpp) (libkernel) are kept in the NTFS extended attribute `$LXMOD`, the name WSL uses for a Linux mode. `mkdir`, `mkdirat`, `sceKernelMkdir`, an `open` that creates a file, `chmod` and `fchmod` write it; `stat`, `lstat` and `fstat` read it. A file or directory without it, because the host created it or its volume has no extended attributes (FAT32, exFAT), reports the mode its attributes give: `0777` for a directory, `0666` for a file, `0444` for a read-only one; on such a volume `chmod` keeps only the read-only attribute. The owner and group are not kept, and the host does not check the bits on access.
