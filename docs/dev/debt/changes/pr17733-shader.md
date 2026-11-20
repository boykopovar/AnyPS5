# shader

## add Functional

- [f32 NaN source selection](../../../core/shader/recompiler/SpirvBackend/src/SpirvFlowEmitter.cpp) uses `NanPropagateF32`, which the SRT tape compiler does not handle: descriptor arithmetic that reaches it falls back to the interpreter. Source repair branches on a NaN host result; multiplication by a known normal constant retains the frontend proof and source selection. Console compatibility and performance outside the measured workloads remain unverified.
- [Listed relative window reads](../../../core/shader/recompiler/Optimization/src/VectorWindowIndexNarrower.cpp) retain dynamic indexing when a window is written, a relative index cannot be listed within 16 values and eight recursive levels, or any listed value is outside the window. Otherwise reads become constant-slot reads chosen by compares. The compare-chain cost for programs with many listed reads is unmeasured.
