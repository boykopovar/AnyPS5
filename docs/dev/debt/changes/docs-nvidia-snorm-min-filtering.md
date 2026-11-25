# libSceAgcDriver

## add Functional

- [SNORM textures with the most negative code](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) are filtered by the host sampler. On NVIDIA's Windows driver (RTX 5080, 610.74) a bilinear `image_sample_lz` weights 8_SNORM, 8_8_8_8_SNORM and 16_SNORM texels of -128 or -32768 as -128/127 or -32768/32767 and clamps only the result to -1. Between codes 0x80 and 0x00 at weight 0.5 it returns -0.503922 (16-bit: -0.500015), and between 0x80 and 0x7f it returns -0.003937. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver clamp each texel to -1 before weighting and return -0.5 and 0. Point sampling and texels of -127 or above match.
