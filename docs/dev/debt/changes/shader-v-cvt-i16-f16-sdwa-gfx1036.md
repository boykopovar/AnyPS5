# shader

## change [SDWA byte destinations on `v_cvt_i16_f16`]
- [SDWA byte destinations on `v_cvt_i16_f16`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_i16_f16` accepts `SDWA` `BYTE_0`..`BYTE_3` and `WORD_0`/`WORD_1` destinations with `PRESERVE`, `PAD`, and `SEXT` unused-bit modes (for `SDWA` `BYTE_0..BYTE_3` / `WORD_0..WORD_1`, `dst_unused = PRESERVE` writes only the selected lane, `PAD` zeroes the rest, and `SEXT` sign-extends the selected byte/word into the upper bits), matching RDNA2 (`gfx1036`) hardware measurements on `BYTE_0..BYTE_3` (`-3.0`, `10.0`, `100.0` with `PRESERVE`, `PAD`, and `SEXT`).
