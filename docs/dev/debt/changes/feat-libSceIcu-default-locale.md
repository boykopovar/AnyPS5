# libSceIcu

## remove | `uloc_getDefault` | `m1nD9D9LmP0` |

## add Functional

- [Default locale](../../../core/libs/prx/libSceIcu/DefaultLocale.cpp) - the PS5 default locale is not measured. `uprv_getDefaultLocaleID`, `uloc_getDefault` and `uloc_setDefault` follow the POSIX branch of ICU 68.2 [putil.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp) and [locid.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/locid.cpp): guest `setlocale(6, NULL)`, then `LC_ALL`, `LC_MESSAGES`, `LANG`, else `en_US_POSIX`, read once. Unverified: that the console build takes that branch, `LC_MESSAGES` = 6 (FreeBSD) and the console environment. The ID goes through this module's `uloc_canonicalize`, not `ulocimp_canonicalize`, nor `uloc_getName` again. `uloc_setDefault` with a non-null ID throws: ICU needs `uloc_getName`, not implemented. Closes when: `uloc_getName` is implemented and compared with the console.
