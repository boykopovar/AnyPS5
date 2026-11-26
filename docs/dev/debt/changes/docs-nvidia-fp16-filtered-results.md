# libSceAgcDriver

## add Functional

- [Bilinear samples of 16-bit float, packed float and sRGB textures](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) come from the host sampler at its precision. NVIDIA's Windows driver (RTX 5080, 610.74) returns the `image_sample_lz` result of 16_FLOAT, 16_16_FLOAT, 16_16_16_16_FLOAT, 11_11_10_FLOAT and 9_9_9_5_FLOAT textures rounded to the nearest half-precision value, and of 8_SRGB and 8_8_8_8_SRGB textures with at most half-precision mantissas (up to 0.0045 off). RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) returns the full single-precision weighted sum, and AMD's Windows driver matches it bit for bit. 32_FLOAT results match on all three; 8_UNORM results differ on NVIDIA by up to 7.5e-6.
