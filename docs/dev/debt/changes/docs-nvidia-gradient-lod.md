# shader

## add Functional

- [Sample LOD on NVIDIA](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) comes from the host sampler, whose λ depends on the direction of the gradients. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver return log2 of the gradient length within 1/256 at every angle. For `image_sample_d` and `image_sample_cd` gradients 2^1.4 texels long, turned in 2.8125° steps, an NVIDIA GeForce RTX 5080 (610.74) gives λ 1.40 along an axis, up to 1.48 near 20° and 70°, and down to 1.32 at 45° with one non-zero gradient (1.36 with two perpendicular ones), read through its 1.75 mip blend slope. A nearest-mip sample near a level boundary can therefore read another level. The PS5's gfx1013 is not measured.
