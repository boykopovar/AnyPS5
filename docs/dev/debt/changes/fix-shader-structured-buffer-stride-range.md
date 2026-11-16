# shader

## add Functional

- [Swizzled V# subword accesses](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - the swizzled address and range of `buffer_load_ubyte` and `buffer_load_ushort` (V# word 1 bit 31 with a nonzero stride, index stride 8) are not pinned. On RDNA2 (gfx1036, AMD Windows driver 32.0.21043.5001, [hardware oracle](../HW_ORACLE.md); IEEE 0, DX10 clamp 1, denorm32 0, denorm16 3, round32/16 0, fp16 overflow 0) with stride 16 and index 0 the byte and the dword at offset 0 both read 7, not the bytes at the documented swizzled address 0 (0x81, 0x968f8881) or at any address the linear model predicts, and the other offsets read 0 or small values that identify no source byte. The emitted swizzle formula is unchanged. Closes when: the swizzled layout is measured with a source buffer that identifies each byte.
