# shader

## add Functional

- [f32 NaN results past the per-program budget](../../../core/shader/recompiler/Translation/src/TranslationContext/OperandAccess.cpp) (128, `APS5_NAN_BRANCH_BUDGET`) use the select chain instead of the branching `NanPropagateF32` form. The run-time cost of the select form on programs with more than 128 NaN results is measured only on PPSA21564 (no GPU cost seen) and unmeasured elsewhere.
