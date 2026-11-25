# shader

## add Functional

- [`buffer_store_format_*`](../../../core/shader/recompiler/Translation/src/MemoryInstructions/BufferInstructions.cpp) stores the data components as if the V# DST_SEL were X,Y,Z,W. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md) on coarse-grained memory) applies the V# DST_SEL by the channel rule recorded for `StoreTexel` image stores: 8_8_8_8 and 32_32_32_32 UINT store (w, z, y, x) for W,Z,Y,X and (1, w, 0, y) for Z,0,X,1, 32_UINT stores w whenever DST_SEL X is not X, and 16_16_UINT stores (x, w) even for X,Y,Z,W. AnyPS5 stored (x, y, z, w) in every case on an NVIDIA GeForce RTX 5080 (driver 610.74) and on gfx1036, read back by an identity `buffer_load_format_xyzw`. DST_SEL X,Y,Z,W agrees for four- and one-channel formats, and X,X,X,X for one-channel ones. The PS5's gfx1013 is not measured.
