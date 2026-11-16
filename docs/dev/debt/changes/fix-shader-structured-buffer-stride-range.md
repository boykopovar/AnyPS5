# shader

## add Functional

- [Structured buffer range of unaligned accesses](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - with `OOB_SELECT` 0 an access is in range when `index < NUM_RECORDS` and `offset + size <= stride`, measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21043.5001, [hardware oracle](../HW_ORACLE.md)), where an unaligned `buffer_load_dword` reads the exact bytes, so that driver runs alignment mode UNALIGNED. The console runs mode DWORD (see V# bases off a DWORD boundary), which may align an unaligned dword before the range check, so a dword at an offset that is not a multiple of 4 near the end of a record is unmeasured there. Closes when: a gfx1013 kernel on a PS5 loads a dword at `stride - 3` of a structured V#.
