# libSceAgcDriver

## add Functional

- [4_4_4_4 unorm ROUND_MODE](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - measured on RDNA2 (gfx1036 under RADV, RB+ off, 32_ABGR exports), not on the console's gfx1013: with `CB_COLOR_INFO.ROUND_MODE` set the CB truncates each export to `floor(clamp(v, 0, 1) * 15)` (NaN to 0), and with it clear rounds to nearest. The pixel shader packs a 4_4_4_4 target that way. PS4 (GCN) color targets do not read `ROUND_MODE` and always round.
