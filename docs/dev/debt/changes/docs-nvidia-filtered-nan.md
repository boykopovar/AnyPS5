# libSceAgcDriver

## add Functional

- [NaN in bilinear samples of float textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) takes the host sampler's bits. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) returns 0xffc00000 from a bilinear `image_sample_lz` of a 32_FLOAT or 16_FLOAT texture when a NaN texel has a nonzero weight (also a canonical NaN texel 0x7fc00000), and when +inf and -inf are blended; AMD's Windows driver returns the same. NVIDIA's Windows driver (RTX 5080, 610.74) returns 0x7fffffff for 32_FLOAT and 0x7fffe000 for 16_FLOAT in those cases. Infinite texels blended with finite ones give the same infinity on all three.
