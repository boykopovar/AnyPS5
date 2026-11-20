# libSceAgcDriver

## add Functional

- [Mip selection and blending on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) are the host sampler's. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) weights the next level of an `image_sample_l` by frac(LOD) with a linear mip filter and switches to it at frac(LOD) = 0.5 with a point one, and AnyPS5 on that GPU matches at every LOD from 0 to 2.5 in 1/16 steps. On an NVIDIA GeForce RTX 5080 (driver 610.74, default driver settings) the same samples weight the next level by min(1, 1.75 × frac(LOD)), so LOD 0.5 reads 87.5% of level 1, and a point mip filter switches past frac(LOD) ≈ 0.29 (level 1 from LOD 0.3125, level 0 at 0.25). The S# LOD range and bias move the LOD the same on both GPUs. The PS5's gfx1013 is not measured.
