# shader

## change [SDWA integer bit scans and reverse]

- [SDWA integer bit scans and reverse](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_ffbh_u32`, `v_ffbl_b32`, `v_ffbh_i32` and `v_bfrev_b32` accept all source selects, zero/sign extension and byte/word destinations with pad, sign-extend or preserve: the source is extracted before the 32-bit operation and the result placed after it; the signed scan returns -1 for zero and all ones. On RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) all 96,768 result words, wave32 and wave64 over 32 sources, match the `SdwaIntegerSelectors` model. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.8, 13.3.7 Table 88 and LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp.
