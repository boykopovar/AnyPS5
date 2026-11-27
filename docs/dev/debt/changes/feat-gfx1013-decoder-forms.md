# shader

## add Functional

- [gfx1013-only memory forms](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) (shader recompiler) - the 15 DS 64-bit SRC2 forms and the 61 SMEM store and atomic forms have decoder tables for the gfx1013 reference, but no translation: `TranslationContext::emitMemory` has no case for them, so a shader that uses one still fails with the translator's unsupported-instruction error. The `_X2`/`_X4` suffix gives the SMEM dword count; the 64-bit SRC2 forms use the same DS `Offset` operand as the 32-bit forms (LLVM `DSInstructions.td` `DS_1A`, llvmorg-19.1.0), so they keep the 32-bit offset scale, which is not measured on hardware. Closes when: each form is translated and tested.
