# shader

## change entries and direct T#s under a table S# sample through a point-filtered copy

- Uint and Sint [image table](../../../core/shader/recompiler/SpirvBackend/src/SpirvMemory/SpirvDescriptors.cpp) entries and direct T#s under a table S# sample through a point-filtered copy of the S#: Vulkan leaves linear filtering of integer formats undefined. The PS5's gfx1013 is not measured. On RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), `image_sample_lz` with a bilinear S# returns the same nearest texel as a point S# for 8-, 16- and 32-bit UINT and SINT T#s; with a linear mip filter they return the nearest level's texel like a point mip filter (LOD 0.5 takes the next level), for 8-, 16- and 32-bit UINT and SINT, `8_8_8_8_UINT` and `16_16_16_16_UINT`, as the point-filtered copy samples.
