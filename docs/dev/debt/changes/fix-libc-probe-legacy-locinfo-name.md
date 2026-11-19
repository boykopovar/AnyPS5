# libc

## add Unknown function info

- [std::_Locinfo constructor name](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) - the first two bytes of the locale name are read with `CopyGuestString_nid_no_patch`, which is enough to tell `"C"` from every other name; an unreadable name throws the invalid-argument exception an unsupported name already throws, because the console's behaviour on unmapped memory is unmeasured.
