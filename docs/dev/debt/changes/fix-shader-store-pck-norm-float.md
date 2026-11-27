# shader

## add Functional

- [Packed stores to normalized and 16-bit float images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`image_store_pck` and `image_store_mip_pck` to the 8- and 16-bit UNORM and SNORM formats and to 16_FLOAT, 16_16_FLOAT and 16_16_16_16_FLOAT) write the data bits unchanged through a same-size UINT view, as measured on RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md), coarse-grained memory, 32 lanes each of 8_UNORM, 8_SNORM, 16_UNORM, 16_FLOAT, 8_8_UNORM, 16_16_UNORM, 16_16_FLOAT, 8_8_8_8_UNORM, 16_16_16_16_UNORM and 16_16_16_16_FLOAT): components take consecutive bits of the data dwords, with no conversion or saturation. Only write-only images take this path; the PS5's gfx1013 is not measured. Packed loads of the SNORM and 16-bit float formats and packed accesses of the bitfield formats still throw.
