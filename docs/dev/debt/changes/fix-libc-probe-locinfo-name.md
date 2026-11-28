# libc

## add Unknown function info

- [_Locinfo constructor name](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) - the first two bytes of the locale name are read with `CopyGuestString_nid_no_patch`, which is enough to tell `""` and `"C"` from every other name; an unreadable name raises the same "bad locale name" runtime error as a null or unsupported one, because the console's behaviour on unmapped memory is unmeasured.
