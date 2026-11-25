# shader

## add Functional

- [Signed packed images](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (GFX10 formats 31, 35, 38, 42, 45 and 49) lack PS5 measurements. An [RDNA2 comparison](https://github.com/boykopovar/AnyPS5/pull/17623#issuecomment-7085427847) matched the tested loads for all six formats and stores for five. Format 45's 2-bit SNORM stores round exact halves away from zero on RDNA2; the shared typed-buffer conversion rounds them to zero until [#25877](https://github.com/boykopovar/AnyPS5/pull/25877). Format codes follow AMD's RDNA 1 ISA §8.2.8, Table 47. Views use R32_UINT and the existing typed-buffer signed conversion. Sampling, gathers, LOD queries, image tables, packed accesses and selection of a missing W channel throw. Render targets are not added. Store saturation, SNORM NaN conversion, D16 conversion and partial-store zero fill remain unverified on PS5.
