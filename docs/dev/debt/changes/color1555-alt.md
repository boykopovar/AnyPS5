# libSceAgcDriver

## change [1_5_5_5 color targets]

- [1_5_5_5 color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) use A1R5G5B5. Pixel-export order and blend-constant channels still need PS5 measurements. AMD packs X/Y/Z/W in bits 0-4/5-9/10-14/15 ([fpPS4](https://github.com/red-prig/fpPS4) `kSurfaceFormat1_5_5_5` and the packed 16-bit texture measurements). Standard-swap exports exchange X and Z because A1R5G5B5 puts B in the low bits. shadPS4 (PS4) and KytyPS5 (PS5, `5_5_5_1`) also swap red and blue. Alternate-swap UNORM packs B/G/R/A from low to high and matches A1R5G5B5 directly. Other number types and swap orders throw. Tests check the format, mapping and write masks. A host execution test draws 32_ABGR exports in both orders into tiled and linear targets and compares each stored texel to the guest layout. PPSA06699 uses this format.
