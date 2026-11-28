# libc

## add Unknown function info

- [strptime](../../../core/libs/prx/libc/src/Strptime.cpp) (libc) - parses date and time representations using the C locale according to POSIX.1-2008. Compound formats and standard conversion specifiers are handled, and century resolution follows POSIX rules (69-99 map to 1969-1999, 00-68 map to 2000-2068). Specifiers %U, %W, %s, %G, %g, %V, %k, and %l are unimplemented and throw. Guest input strings exceeding 64 KiB or format strings exceeding 4 KiB return null. Unreadable guest input pointers or null pointer arguments return null without modifying the destination structure. Console behavior on unreadable guest memory is unmeasured.
