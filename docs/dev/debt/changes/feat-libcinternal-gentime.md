# libSceLibcInternal

## change `_Genld`, `_Gentime`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Fwprep`, `_Genld`, `_Getcloc`, `_Getctyptab`, `_Getdst`, `_Getfld`, `_Getfloat`, `_Getgloballocale`

## add Functional

- [_Gentime](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) follows a [reading of firmware 13.42's libSceLibcInternal.sprx](https://github.com/boykopovar/AnyPS5/issues/21874#issuecomment-6957926742) with the module's C-locale `_Tinfo`, run natively, not on a console. It formats one `strftime` conversion: numbers through Dinkumware's digit writer, which prints values below 0 as zeros and keeps the last digits; names as fields of the `:`-separated `_Tinfo` strings; `%c`, `%x`, `%X`, `%r`, `%D`, `%F`, `%R` and `%T` as a format returned with a negative length; `%z` from `sceKernelConvertLocaltimeToUtc`. The `E`/`O` qualifier is ignored. With `main`'s C locale every format field of `_Tinfo` holds the same string, so which field `%c`, `%x`, `%X` and `%r` read in another locale wasn't settled; this uses `_Formats`. `%U`, `%W` and the ISO week codes use the C99 rules, which the reading checked only on valid dates. Unreadable arguments throw.
