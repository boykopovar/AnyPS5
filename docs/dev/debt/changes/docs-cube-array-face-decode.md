# shader

## add Functional

- [Cube layers](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`CubeLayer`) map a DIM CUBE face coordinate z to the host layer `(z / 8) * 6 + z % 8`, right only for faces 0 to 5 of a cube inside the T#. RDNA2 (gfx1036, AMD driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) truncates z to a signed integer, clamps `z & 7` to 5 and `z >> 3` to the T#'s cubes. On 64x64 32_FLOAT cubes (`DEPTH` 5, 11 and 17, `BASE_ARRAY` 0 and 6), `image_sample_lz` with z 6 or 7 reads face 5 where AnyPS5 reads the next cube's face 0 or 1; z -1, -2 and -9 read face 5 of cube 0 where AnyPS5 reads face 0; a cube past the last (z 8 to 12 with one cube; 16, 24 and 1e9 with two) reads that face of the last cube where AnyPS5 reads its face 5. `image_load` matched on every row. The PS5's gfx1013 is not measured.
