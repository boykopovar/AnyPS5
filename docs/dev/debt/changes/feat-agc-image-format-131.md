# shader

## change [Image format 131]

- [Image format 131](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) (shader recompiler) - T# FORMAT 131 (`Format6E4Float`) works only for an `image_load` without a sampler; a write, packed MIMG access, atomic, sample, gather, LOD query and DCC fast-clear codes 0001, 1110 and 1111 are refused. It is decoded in the shader from an `R32_UINT` image, as no Vulkan format has its layout, measured with `image_load` on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), not on the PS5's gfx1013: R, G and B are bits 0-9, 10-19 and 20-29, each an unsigned float with a 4-bit exponent (bias 7) over a 6-bit mantissa, denormal at exponent 0 and finite at exponent 15 (0x3ff reads 508); A is bits 30-31 as UNORM. Not measured: a denormal's value (decoded as the mantissa times 2^-12), the rounding of the 2-bit alpha (within 1 ULP of alpha/3) and the alpha of a D16 load.
