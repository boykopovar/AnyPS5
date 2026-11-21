# shader

## add Functional

- [NaN texels through the sampler](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp): NVIDIA (GeForce RTX 5080, driver 610.74) returns 0x7fffffff (32_FLOAT) or 0x7fffe000 (16_FLOAT) from `image_gather4` for every NaN texel. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, measured with the [hardware oracle](../HW_ORACLE.md)) converts the texel bit for bit (0x7c01 as 0x7f802000, 0xfdee as 0xffbdc000), as `image_load` does on both. Every 32-bit sample result returns 0xffc00000 when it is NaN, as RDNA2's point and bilinear `image_sample_lz` of 16_FLOAT and 32_FLOAT textures do, including a NaN texel with a nonzero weight and a blend of +inf and -inf. Other sample opcodes and converted formats follow that rule unmeasured, and D16 sample results keep the host's NaN. The PS5's gfx1013 is not measured.
