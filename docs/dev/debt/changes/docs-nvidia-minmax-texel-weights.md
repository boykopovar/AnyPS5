# libSceAgcDriver

## add Functional

- [Min/max reduction on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) reduces the texels the host sampler picks. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver reduce a texel of a bilinear `image_sample_lz` once its weight on each axis exceeds 1/512, whatever its weight on the other axis (1/64 to 511/512 measured). On an NVIDIA GeForce RTX 5080 (610.74), the texel is reduced only when the product of its two axis weights, each rounded to 1/256, is at least 1/512. With an x weight of 1/4 a y weight of 24/4096 is the first to count (not 10/4096); with x 3/256, 680/4096; with x 1/16 nothing up to 62/4096. A 32_FLOAT `FILTER_MODE` 1 or 2 sample can then return another texel. The PS5's gfx1013 is not measured.
