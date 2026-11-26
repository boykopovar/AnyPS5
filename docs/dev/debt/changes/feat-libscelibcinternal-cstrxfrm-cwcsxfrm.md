# libSceLibcInternal

## add Functional

- [_CStrxfrm and _CWcsxfrm](../../../core/libs/prx/libSceLibcInternal/Export.cpp) are unverified against a retail console trace. Linear identity transformation delegates to libc `strxfrm` and `wcsxfrm` in the classic "C" locale, matching POSIX semantics where `strcmp` and `wcscmp` on transformed results preserve collation order. Closes when: compared against retail console module exports under non-C locales.
