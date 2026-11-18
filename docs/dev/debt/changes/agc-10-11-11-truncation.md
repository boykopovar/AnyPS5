# libSceAgcDriver

## change [10_11_11 unorm color targets]

- [10_11_11 unorm color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - the CB conversion is measured on RDNA2 (gfx1036 under RADV, COLOR_10_11_11 with the UNORM number type, RB+ off, 32_ABGR exports), not on the console's gfx1013: with `CB_COLOR_INFO.ROUND_MODE` clear an export rounds to nearest (a 32-bit input never falls on a tie), and with it set truncates to `floor(clamp(v, 0, 1) * max)`, 2047 for the 11-bit fields and 1023 for the 10-bit one; NaN gives 0. The pixel shader packs each export that way into an R32_UINT attachment. PS4 (GCN) color targets do not read `ROUND_MODE` and always round.
