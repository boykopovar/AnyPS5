# libSceAgcDriver

## add Functional

- [Bilinear texel weights on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) are the host sampler's. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver weight a texel of a 2D bilinear `image_sample_lz` by the exact product of its two axis weights, each a multiple of 1/256. On an NVIDIA GeForce RTX 5080 (610.74) that product is rounded to the nearest 1/256, halves up: on a 2x2 32_FLOAT texture, axis weights 77/256 and 26/256 give 2048/65536 instead of 2002/65536, 3/256 and 3/256 give 0 instead of 9/65536, and 253/256 and 128/256 give 32512/65536 instead of 32384/65536. 1D weights match. Bilinear `image_sample_c_lz` depth comparisons weight their passing texels the same way. A texel's weight can then be off by up to 1/512; a constant texture still reads back exactly. The PS5's gfx1013 is not measured.
