# shader

## add Functional

- [Typed 16-bit buffer loads at odd offsets](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - `tbuffer_load_format_x` with a 16-bit data format at an odd byte offset is not pinned. On RDNA2 (gfx1036, AMD Windows driver 32.0.21043.5001, [hardware oracle](../HW_ORACLE.md); IEEE 0, DX10 clamp 1, denorm32 0, denorm16 3, round32/16 0, fp16 overflow 0) a structured V# with stride 16 returned 0 at offsets 1, 3, 7, 11 and 15 and the data at the even offsets up to 14; the recompiler's result at odd offsets is not measured. Closes when: odd-offset typed 16-bit loads are measured in raw and structured buffers and pinned in an execution test.

- [16-bit buffer loads across a dword](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - a `buffer_load_ushort` or `buffer_load_sshort` at an offset of 3 mod 4 returns both bytes, as measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21043.5001, [hardware oracle](../HW_ORACLE.md)), where an unaligned `buffer_load_dword` also reads the exact bytes, so that driver runs alignment mode UNALIGNED. The console runs mode DWORD (see V# bases off a DWORD boundary); whether DWORD mode splits or aligns a 16-bit access that crosses a dword is unmeasured. Closes when: a gfx1013 kernel on a PS5 reads a short at offset 3 mod 4.
