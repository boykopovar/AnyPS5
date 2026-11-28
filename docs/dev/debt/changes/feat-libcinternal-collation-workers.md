# libSceLibcInternal

## remove `_Wcscollx` (NID `wmkXFgerLnM`)

## add Functional

- [_Strxfrmx, _Wcscollx, _Wcsxfrmx and _Wcsftime](../../../core/libs/prx/libSceLibcInternal/CollationWorkers.cpp) (libSceLibcInternal, NIDs `CpiD2ZXrhNo`, `wmkXFgerLnM`, `cmIyU0BNYeo`, `RGc3P3UScjY`) - not checked against `libSceLibcInternal.sprx` or on a console. They ignore the collation state or locale, so custom tables are not applied and only the C locale is supported. Prototypes follow Dinkumware declarations in QNX's [xstrxfrm.h](https://github.com/LongJohnCoder/openqnx/blob/cc95df3543a1c481d4f8ad387e29f0d1ff0c87fe/trunk/lib/c/inc/xstrxfrm.h), [xwcsxfrm.h](https://github.com/LongJohnCoder/openqnx/blob/cc95df3543a1c481d4f8ad387e29f0d1ff0c87fe/trunk/lib/c/inc/xwcsxfrm.h) and FreeBSD `wcsftime.c`, with 16-bit `wchar_t`. They delegate to libc's `strxfrm`, `wcsxfrm`, `wcscoll` and `wcsftime`. Null strings or format/time pointers, and a null destination with a nonzero count, throw.
