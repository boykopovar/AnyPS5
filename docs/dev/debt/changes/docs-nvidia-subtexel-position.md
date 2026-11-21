# libSceAgcDriver

## add Functional

- [Subtexel positions on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) are the host sampler's. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver round a bilinear `image_sample_lz`'s texel position u × width − 0.5 to the nearest 1/256, ties to even, on 32_FLOAT textures 1024 to 16384 texels wide or high. An NVIDIA GeForce RTX 5080 (610.74) rounds ties up on textures up to 4096 texels, and truncates to 1/256 on 8192- and 16384-texel axes, so an axis weight there is up to 1/256 lower: at x = 1946.9247 on an 8192-wide texture it takes 236/256 where RDNA2 takes 237/256. The PS5's gfx1013 is not measured.
