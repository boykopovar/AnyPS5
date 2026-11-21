# libc

## add Unknown function info

- [setlocale name](../../../core/libs/prx/libc/src/StdioExtras.cpp) (libc) - the first two bytes of the locale name are read with `CopyGuestString_nid_no_patch`, which is enough to tell `""` and `"C"` from every other name; an unreadable name returns null like an unsupported one, because the console's behaviour on unmapped memory is unmeasured.
