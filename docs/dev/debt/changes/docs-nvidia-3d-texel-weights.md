# libSceAgcDriver

## add Functional

- [Trilinear 3D texel weights on NVIDIA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) are the host sampler's. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver weight a texel of a DIM 3D `image_sample_lz` with bilinear XY and linear `Z_FILTER` by the exact product of its three 8-bit axis weights. An NVIDIA GeForce RTX 5080 (610.74) rounds the x times z weight to the nearest 1/256, halves up, then rounds its product with the y weight the same way. On a 2x2x2 32_FLOAT texture, axis weights x 77/256, y 128/256, z 128/256 give 20/256 where the exact product is 19.25/256, and x 128/256, y 77/256, z 128/256 give 19/256. All 48 measured weight triples follow this rule. The PS5's gfx1013 is not measured.
