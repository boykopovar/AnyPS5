# shader

## add Functional

- [gfx1013-only memory forms](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) (shader recompiler) - the 15 DS 64-bit SRC2 forms and the 61 SMEM store and atomic forms have decoder tables for the gfx1013 reference, but no translation: `TranslationContext::emitMemory` has no case for them, so a shader that uses one still fails with the translator's unsupported-instruction error. The 64-bit SRC2 offset follows the 32-bit convention, with the second operand read from the secondary offset, and the `S_*_X2`/`S_*_X4` dword counts come from the reference rows. Closes when: each form is translated and tested.
