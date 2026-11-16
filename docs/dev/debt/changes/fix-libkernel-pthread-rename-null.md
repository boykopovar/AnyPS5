# libkernel

## change [scePthreadRename, measured]

- [scePthreadRename, measured](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp) (libkernel) - not modelled: the console sets `errno` to 63 or 14 on the calling thread for a name of 32 or more bytes or an unreadable name (firmware 13.590.001, payload process, 2026-11-11), where AnyPS5 leaves `errno` unchanged; a title process was not measured. A null or invalid thread returns `ESRCH` before the name is checked, and a null name returns 0 and leaves an empty name, as on that payload process. Measured on a payload process on firmware 11.60 (2026-11-08): a 40-character name returns `ENAMETOOLONG` (`0x8002003f`), and 5 and 31 characters are accepted while 32, 33, 40 and 64 are refused, keeping the old name. The limit here is 32 bytes, as `scePthreadCreate` was measured to have (31 kept, 32 refused). `pthread_set_name_np` with 32 to 64 characters kept the old name and set `errno` to `ENAMETOOLONG`.

## change [pthread_rename_np, scePthreadRename name]

- [pthread_rename_np, scePthreadRename name](../../../core/libs/prx/libkernel/Pthread/Posix/Thread.cpp) (libkernel) calls `scePthreadRename`, so the two share the lookup order measured on firmware 13.590.001 (payload process; `scePthreadRename` on 2026-11-11, `pthread_rename_np` on 2026-11-13): a null or invalid thread returns `ESRCH` (3) before the name is read, a null name returns 0 and clears the name, and an unreadable name returns `EFAULT` (14) and a name of 32 or more bytes `ENAMETOOLONG` (63), each keeping the old name. The name is copied with `CopyGuestString_nid_no_patch`, and a failed host allocation returns `ENOMEM`; the copy reads at most 32 bytes, so a longer name never reads past the 32nd byte. FreeBSD 11.4's libthr `pthread_set_name_np` also finds the thread before it calls `thr_set_name`. The console's `errno` for the two name errors is not modelled (see `scePthreadRename`).

# libkernel_sys

## change [sceKernelGetThreadName]

- [sceKernelGetThreadName](../../../core/libs/prx/libkernel_sys/ThreadQueries.cpp) (libkernel_sys) - joined-thread `ESRCH` and leaving errno unchanged are unmeasured. Only this process's guest thread ids are resolvable; other-process ids return `ESRCH` here, whereas the PS5 probe resolved them. `scePthreadRename` follows a console probe of it (firmware 13.590.001, payload process, its own thread): 31 bytes are accepted; 32 or more return `0x8002003F` (`ENAMETOOLONG`) and keep the previous name; an unreadable name pointer returns `EFAULT`, and an invalid thread handle returns `ESRCH` before any name check. A NULL name succeeds and clears the name, and a NULL or invalid thread handle returns `ESRCH`, as measured there and modelled by `scePthreadRename`. A failed host allocation for the name copy returns `ENOMEM`.

# libScePosixForWebKit

## change [pthread_setname_np]

- [pthread_setname_np](../../../core/libs/prx/libScePosixForWebKit/Export.cpp) (libScePosixForWebKit) stores the guest thread name in libkernel's thread object, so `pthread_getname_np` returns it. It does not call libkernel's `pthread_rename_np`: a null thread or name returns `EINVAL` (22) here, while libkernel's now returns `ESRCH` for a null thread and clears the name for a null name, as measured. `ENAMETOOLONG` (63) from 32 bytes on, `0` otherwise, with the name truncated to 31 bytes when read back. It does not apply the Windows thread description or the job affinity that `scePthreadRename` sets, because those helpers are private to libkernel; the console's exact error numbers for this module are unverified
