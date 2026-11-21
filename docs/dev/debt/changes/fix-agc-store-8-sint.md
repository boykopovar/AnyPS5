# shader

## change [SINT storage images]

- [SINT storage images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) of 8- and 16-bit components saturate each stored 32-bit value to the component range, as adriadam10 measured for image stores on RDNA2 (gfx1035) in #944 (448 rows match); the PS5's gfx1013 is not measured. A D16 store sign-extends each 16-bit half before that saturation, as measured on RDNA2 (gfx1035) for `image_store d16` to all eight SINT formats with DMASK 0xf, 0x7, 0x5, 0x3, 0x2, 0xa and 0x1 (1792 rows match). 8_SINT is written through an R8_UINT view the same way; its saturation to -128..127 was measured on gfx1036 with the [hardware oracle](../HW_ORACLE.md) for 32 values from 0x80000000 to 0x7fffffff.
