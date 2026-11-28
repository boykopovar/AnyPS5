# shader

## add Functional

- [16-bit buffer stores across a dword](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - a `buffer_store_short` or `buffer_store_short_d16_hi` whose two bytes fall in different dwords writes both bytes in every stage, as compute stages already do through byte stores. The hardware result is not measured: the PS5 runs alignment mode DWORD (see V# bases off a DWORD boundary), and whether it splits, aligns or drops a 16-bit store that crosses a dword is unknown. Closes when: a gfx1013 kernel on a PS5 stores a short at offset 3 mod 4 and the result is pinned in an execution test.
