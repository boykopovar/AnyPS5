# libSceAgcDriver

## add Functional

- [BC1 to BC5 textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) are decoded by the host. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) interpolates BC1 colors on the bit-replicated 8-bit endpoints a and b as (43a + 21b + 32) >> 6 and (21a + 43b + 32) >> 6, and (a + b + 1) >> 1 in three-color blocks; AMD's Windows driver matches RDNA2 bit for bit in every BC format. NVIDIA's Windows driver (RTX 5080, 610.74) differs in point samples of random blocks by up to 3/255 (BC1), 5.5/255 (BC2), 6/255 (BC3, BC4 UNORM, BC5 UNORM), 11/255 (BC4 SNORM) and 16/255 (BC5 SNORM), in 21% to 85% of the channel values, and by 1/255 in 4% of BC7 sRGB ones. BC6H and BC7 UNORM match.
