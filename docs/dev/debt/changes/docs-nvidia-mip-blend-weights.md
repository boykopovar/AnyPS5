# libSceAgcDriver

## add Functional

- [Mip selection and blending on the host](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) come from the host sampler. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) weights the next level of an `image_sample_l` by frac(LOD) rounded to 1/256, halves to even, and a point mip filter switches to it at 0.5, with any anisotropy or `PERF_MIP`. An NVIDIA GeForce RTX 5080 (610.74, default driver settings) weights it by min(1, 1.75 × frac(LOD)), both truncated to 1/256, so LOD 0.5 reads 87.5% of level 1, and a point mip filter switches past 0.29; at `MAX_ANISO_RATIO` 2x, 4x, 8x and 16x the slope is 2, 2.5, 3 and 4. AMD's Windows driver matches RDNA2 without anisotropy; with it the weight is 0.5 + s × (frac(LOD) − 0.5), clamped to 0 to 1, with s 1.25, 1.5, 2 and 2. The S# LOD range and bias move the LOD the same on these hosts. The PS5's gfx1013 is not measured.
