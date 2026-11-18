# libkernel_sys

## change [kldfind, kldfirstmod, kldnext and kldunload]

- [kldfind, kldfirstmod, kldnext, kldstat and kldsym](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) fail with `ENOSYS` (78) and return -1 without reading their arguments or writing anything. A PS5 payload process (firmware 11.60, probes `libkernel_sys_kldstat_readonly` build `a1168cfc555b` and `libkernel_sys_kld_family_readonly` build `60a0e9b7c6e8`) answered `ENOSYS` to `kldnext(0)`, `kldfind("kernel")`, `kldfirstmod` with file ids 1 and 0x7fffffff, `kldstat` with file ids 1 and 0x7fffffff and versions 2080, 0 and 4, and `kldsym(0, KLDSYM_LOOKUP, "printf")`, and left the output of `kldstat` and `kldsym` unchanged, so those kernel calls are not implemented there and the FreeBSD argument checks are never reached. Null or unreadable pointers, names of 1024 bytes or more, negative file ids and other commands were not measured; here they also get `ENOSYS`. A title process may answer differently, and what credentials the payload process has was not measured

## remove [kldsym]

## remove [kldstat]

## change [kldload]

- [kldload and kldunload](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follow FreeBSD 11.4's [`kern_kldload`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_linker.c) for a jailed, non-root caller: the `kldload` name is copied in first (`EFAULT` for a null or unreadable pointer, `ENAMETOOLONG` at 1024 bytes), then the securelevel and `PRIV_KLD_LOAD` checks fail with `EPERM` before any search or open, so a missing file, an existing file and a file that is not an ELF module all give `EPERM`; `kldunload` returns `EPERM` for every file id, as a sandboxed or jailed title lacks `PRIV_KLD_UNLOAD`. That a title runs jailed is taken from `prison_priv_check`, which grants no `PRIV_KLD_LOAD`; neither call is measured on a console, and the other kld calls are `ENOSYS` in a payload process, so a title may see `ENOSYS` here too
