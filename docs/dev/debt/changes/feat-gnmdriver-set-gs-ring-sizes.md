# libSceGnmDriver

## add Unknown function info

- [sceGnmSetGsRingSizes](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - not measured on a console. The two ring sizes in bytes (ES-GS, then GS-VS) and their limits come from [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/264d6b71d8671a429ad2c9d1a281e18558fb9693/src/core/libraries/gnmdriver/gnmdriver.cpp#L1765-L1782): each must be 4 MB to 8 MB and 1 MB aligned. Sizes in that range return 0 and change nothing, because the host geometry path sizes its own storage per draw from `VGT_ESGS_RING_ITEMSIZE` and has no fixed ES-GS or GS-VS ring to resize. Any other size throws: shadPS4 returns 0x08000000 for it, which no firmware disassembly or console run confirms.
