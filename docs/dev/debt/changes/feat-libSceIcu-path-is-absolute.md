# libSceIcu

## add Functional

- [`uprv_pathIsAbsolute`](../../../core/libs/prx/libSceIcu/AbsolutePath.cpp) - follows ICU 68.2 [putil.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp#L1334-L1360) as built for a POSIX platform, where `U_FILE_SEP_CHAR` and `U_FILE_ALT_SEP_CHAR` are both `/` and the drive-letter test is compiled out: a path is absolute when it starts with `/`. Unverified: that the console build uses that configuration.
