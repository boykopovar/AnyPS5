# shader

## add Functional

- [Packed stores to SINT images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`image_store_pck`) write the data bits of each component unchanged through a UINT view, without the saturation of `image_store`: 16_SINT, 8_8_SINT and 16_16_SINT take components from consecutive bits of the data dwords (8_8 and 16_16 from one dword), as measured on RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md), DMASK 0x1 and 0x3, 32 values each); the 32-bit SINT formats did already. The PS5's gfx1013 is not measured. 8_SINT, 8_8_8_8_SINT and 16_16_16_16_SINT packed stores still throw: on gfx1036 an 8_8_8_8_SINT store with DMASK 0x3 writes all four bytes of the first data dword.
