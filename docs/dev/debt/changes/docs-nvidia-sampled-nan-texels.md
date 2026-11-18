# shader

## add Functional

- [NaN texels through the sampler](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) keep the host driver's bits. On RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, measured with the [hardware oracle](../HW_ORACLE.md)) a point `image_sample_lz` of a 16_FLOAT or 32_FLOAT NaN texel returns 0xffc00000 whatever the texel's sign and payload, `image_gather4` returns the texel converted bit for bit (0x7c01 as 0x7f802000, 0xfdee as 0xffbdc000) and `image_load` does the same; AnyPS5 on that GPU matches all three. On an NVIDIA GeForce RTX 5080 (driver 610.74) the sample passes the texel's NaN through unchanged and the gather returns 0x7fffffff (32_FLOAT) or 0x7fffe000 (16_FLOAT) for every NaN; loads match. Infinities and ordinary values agree everywhere. The PS5's gfx1013 is not measured.
