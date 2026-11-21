# libSceAgcDriver

## change [4_4_4_4 CB resolves]

- [4_4_4_4 CB resolves](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) (libSceAgcDriver) - a CB resolve whose source is a multisampled 4_4_4_4 unorm color target averages each 4-bit field's codes over the samples, rounding halves up when `CB_COLOR_INFO.ROUND_MODE` is clear and truncating when it is set. This follows gfx1036 measurements under RADV's hardware resolve (RB+ off, 2, 4 and 8 samples), not the console's gfx1013. Which target's `ROUND_MODE` the console uses is not measured, so a resolve between targets whose `ROUND_MODE` or layout differ is refused.
