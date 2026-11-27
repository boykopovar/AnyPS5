# libSceAgcDriver

## add Functional

- [Mipmapped DCC color-target fast clears](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - a pipe-aligned 2D mipmapped DCC color target's viewed mip reads its own key slice, placed as [AMD Addrlib](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/imported/addrlib/src/gfx10/gfx10addrlib.cpp) `Gfx10Lib::HwlComputeDccInfo` lays it out for GFX10: 4096-key blocks of 1 MiB of texels, the mip-tail block first, then the mips above the tail from the smallest to mip 0. The tail layout rests on Addrlib only, not on a title, and a viewed mip in the tail is not fast-cleared. Only 3200x1800 8-byte and 2400x1800 4-byte 5-mip chains were measured; other element sizes and the console result of a clear over stale bytes are not verified. Arrays, 3D and non-pipe-aligned targets still ignore the chain's keys. `APS5_NO_DCC_MIP_FAST_CLEAR` turns the fix off.
