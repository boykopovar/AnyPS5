# libc

## add Functional

- [getenv, setenv, unsetenv and putenv](../../../core/libs/prx/libc/src/Environment.cpp) (libc) first check that the caller's name, value or `putenv` text can be read up to its terminator: names within 64 KiB, values and `putenv` text within 1 MiB. They read it with `GuestCopyIn_nid_no_patch` in page-bounded 512-byte chunks into a stack buffer, so the check allocates nothing. An unreadable string sets `errno` to `EFAULT` (14) and fails as an invalid name does (`getenv` returns null, the others -1). A string with no terminator within its limit fails with `ENAMETOOLONG` (63), where FreeBSD's libc has no limit. This is a host-safety fallback: the console's result for an inaccessible string is not measured. Closes when: compared with the console.
