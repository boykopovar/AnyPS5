# shader

## add Functional

- [Cube samples](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) go through a 2D-array view, with the face coordinates moved from [1, 2] to [0, 1] and the face id as the layer, so a bilinear sample near a face edge clamps to that face's edge texels. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) blends across the edge into the neighbouring face, with `DISABLE_CUBE_WRAP` clear: on a 4x4 32_FLOAT cube addressed through `v_cubesc/tc/ma/id`, `image_sample_lz` with a bilinear filter differed at 9 of 16 directions on an NVIDIA GeForce RTX 5080 (driver 610.74) and on gfx1036, all near an edge or corner. Direction (0.1, 1, 0.9) reads 2624.24 on hardware and 2031.70, a +Y edge texel, through AnyPS5, and (1, 1, 1) 2012.01 against 4003.0. With a point filter every direction matched. The PS5's gfx1013 is not measured.
