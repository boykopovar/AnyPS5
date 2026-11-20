# shader

## add Functional

- [Cube layers](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`CubeLayer`) map a DIM CUBE face coordinate z as RDNA2 (gfx1036, AMD driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) does: z is truncated to a signed integer, the face is `min(z & 7, 5)` and the cube `z >> 3` clamped to the T#'s cubes, measured with `image_sample_lz`, `sample`, `sample_l`, `sample_b` and `gather4` variants on 64x64 32_FLOAT cubes (`DEPTH` 5, 11 and 17, `BASE_ARRAY` 0 and 6). GCN guest shaders keep `(z / 8) * 6 + z % 8`, which is right only for faces 0 to 5 of a cube inside the T#; cube decode is not measured on GCN hardware. z outside the 32-bit integer range, where `OpConvertFToS` is undefined, and the PS5's gfx1013 are not measured.
