# libSceAgcDriver

## add Functional

- [S# FILTER_MODE 3](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - S# word 0 `FILTER_MODE` 3 is sampled as a weighted average, like 0, and [materialized as 0](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) so the min/max rules do not apply to it. Not measured on a PS5. On an RX 6650 XT (gfx1032, `tools/hw-oracle`), `image_sample_lz` through a bilinear sampler on a 64x64 8_8_8_8_UNORM gradient at 32 coordinates 0.3 texel off the centres returns exactly what `FILTER_MODE` 0 returns, while 1 and 2 differ from 0 and from each other at 28 of them.
