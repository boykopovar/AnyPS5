# libkernel_sys

## change [link]

- [link](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) and `linkat` count the symbolic links met while resolving the source and the new name, and return `ELOOP` at the 33rd, as FreeBSD 11.4's [`namei`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/vfs_lookup.c#L484-L487) does with `MAXSYMLINKS` (32), where the hosts allow longer chains (Linux 40). The count walks the host path from its root, so symbolic links in the host directories a guest path maps to count too; only Windows symbolic links count there, not junctions. A guest path, and on Windows a descriptor-relative one, reaches the count with its `..` components already dropped lexically, so a link a dropped component would have followed is not counted. Host hard-link errors return guest errno, including EACCES and EXDEV; unmapped Windows errors become EIO. Race-time precedence is unverified on console.
