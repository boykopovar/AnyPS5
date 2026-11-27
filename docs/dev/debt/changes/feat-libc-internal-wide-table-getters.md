# libSceLibcInternal

## change [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Getint`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Getint`, `_Getstr`, `_Gettime`, `_Getzone`

## add Functional

- [_Getpwctytab and _Getpwctrtab](../../../core/libs/prx/libSceLibcInternal/WideCharacterTables.cpp) are not measured on a console or verified beyond SDK 10.00. The C-locale ranges follow the public ASCII classification and case rules in [GuestRune::Type and MakeDefault](https://github.com/boykopovar/AnyPS5/blob/37646cd2caa0eb5edef99bc9bfacf19b2b089c6d/core/libs/prx/libc/src/RuneLocale.cpp). SDK 10.00 `libc.prx` at `0x3b900` and `0x3bb90`, its relocated tables and the `_Iswctype`/`_Towctrans` consumers establish the pointer return, 16-byte rows, property order and 16-bit range triples. These fixed tables do not add locale selection. The SDK's legacy property indices differ from the existing FreeBSD-style `wctype` masks; that separate API is unchanged.
