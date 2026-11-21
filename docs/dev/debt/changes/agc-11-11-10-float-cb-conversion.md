# libSceAgcDriver

## change [11_11_10 float color targets]

- [11_11_10 float color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - the CB conversion is measured on RDNA2 (gfx1036 under RADV, COLOR_11_11_10, RB+ off, 32_ABGR exports), not on the console's gfx1013: an export truncates toward zero whether `CB_COLOR_INFO.ROUND_MODE` is set or clear, overflows to the largest finite value, keeps +infinity, turns negatives and -infinity into zero and turns every NaN into `0x3f0` (10-bit field) or `0x7e0` (11-bit field). The pixel shader packs each export that way into an R32_UINT attachment. Only the standard (COMP_SWAP 0) and reversed (COMP_SWAP 2) component orders are accepted, putting red and blue in the 10-bit low field. That follows from the format's layout and from the standard swap Mesa pairs with COLOR_10_11_11 for R11G11B10_FLOAT (ac_formats.c); it was not measured on the console.
