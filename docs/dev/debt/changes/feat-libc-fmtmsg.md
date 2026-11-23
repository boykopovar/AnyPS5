# libc

## add Unknown function info

- [fmtmsg](../../../core/libs/prx/libc/src/Fmtmsg.cpp) (libc) - the `MM_*` values follow glibc's [`fmtmsg.h`](https://github.com/bminor/glibc/blob/master/stdlib/fmtmsg.h), since POSIX names them without numbers, and are not verified against the PS5 header. The stderr layout follows the example in POSIX [`fmtmsg`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/fmtmsg.html) (`label: SEVERITY: text`, then `TO FIX: action tag`); POSIX gives no other separators. `MSGVERB` selects components as POSIX describes, and an unknown keyword selects all of them. `MM_CONSOLE` writes nothing, since there is no console, and returns `MM_NOCON`. A severity other than `MM_HALT`, `MM_ERROR`, `MM_WARNING`, `MM_INFO` or `MM_NOSEV` prints no keyword. Only the first line and the TO FIX line are written, and a tag without an action prints on its own line. `addseverity` and the `SEV_LEVEL` environment variable are not implemented.
