# shader

## add Functional

- [Stores to converted three-channel images](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) (11_11_10 and 10_11_11, UNORM and FLOAT) accept a T# DST_SEL that selects W for the fourth channel and write x, y and z as with a constant there: on RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) `image_store` dmask 0xf writes the same bits with DST_SEL X,Y,Z,W, X,Y,Z,1, X,Y,Z,0 and X,Y,Z,X for formats 30, 36, 37 and 43 (32 texels each). A W select in another channel, and any W select on a load or sample, still throws; the PS5's gfx1013 is not measured.
