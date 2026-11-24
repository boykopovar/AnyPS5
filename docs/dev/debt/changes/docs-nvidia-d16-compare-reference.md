# shader

## add Functional

- [Comparison sampling of 16_UNORM images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) passes the reference to the host's depth comparison unchanged, so the host decides how it is rounded to the texel's 16-bit code. RDNA2 (gfx1036 with the [hardware oracle](../HW_ORACLE.md)) and AMD's Windows driver round to the nearest code: `image_sample_c_lz` EQUAL passes for references between codes k - 0.5 and k + 0.5 against a texel of code k (k = 1 and 32768; 32767.5 rounds up to 32768), and LESS fails for all of them. NVIDIA's Windows driver (RTX 5080, 610.74) moves the boundary between codes to between k + 0.55 and k + 0.6: EQUAL fails from k - 0.5 to k - 0.45 and passes from k + 0.51 to k + 0.56, so every function except NEVER and ALWAYS can differ there. 32_FLOAT comparisons match on all three for all eight functions.
