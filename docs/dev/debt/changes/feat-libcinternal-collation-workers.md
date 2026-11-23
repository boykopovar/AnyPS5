# libSceLibcInternal

## change `_Wcscollx` (NID `wmkXFgerLnM`)

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_Wcsftime` (NID `RGc3P3UScjY`); signature and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.

## add Functional

- [_Strxfrmx, _Wcscollx and _Wcsxfrmx](../../../core/libs/prx/libSceLibcInternal/CollationWorkers.cpp) (libSceLibcInternal, NIDs `CpiD2ZXrhNo`, `wmkXFgerLnM`, `cmIyU0BNYeo`) - not checked against `libSceLibcInternal.sprx` or on a console. They ignore the collation state, so tables a caller stores in it are not applied and only the C locale is supported. The prototypes follow the Dinkumware declarations in QNX's [xstrxfrm.h](https://github.com/LongJohnCoder/openqnx/blob/cc95df3543a1c481d4f8ad387e29f0d1ff0c87fe/trunk/lib/c/inc/xstrxfrm.h) and [xwcsxfrm.h](https://github.com/LongJohnCoder/openqnx/blob/cc95df3543a1c481d4f8ad387e29f0d1ff0c87fe/trunk/lib/c/inc/xwcsxfrm.h), with the 16-bit `wchar_t` libc uses. There, `strxfrm`, `wcsxfrm` and `wcscoll` call these workers with the global state, so they delegate to libc's `strxfrm`, `wcsxfrm` and `wcscoll`. Null strings, and a null destination with a nonzero count, throw.
