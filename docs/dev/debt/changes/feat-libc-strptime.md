# libc

## add Unknown function info

- [strptime](../../../core/libs/prx/libc/src/Strptime.cpp) (libc) - parses using the C locale only (POSIX.1-2008 and BSD/GNU extensions `%s`, `%k`, `%l`). Century resolution follows POSIX (69-99 -> 1969-1999, 00-68 -> 2000-2068), `%U`/`%W` and `%G`/`%g`/`%V` require year context to populate calendar fields, `%s` uses local time without adjusting for `%z`, input strings over 64 KiB or format strings over 4 KiB return null, and unreadable guest memory returns null without modifying the destination; console behavior on non-C locales, `%s` with `%z`, and unreadable guest memory is unmeasured.
