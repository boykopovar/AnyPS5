# shader

## add Functional

- [SRT float-to-signed conversion](../../../core/shader/recompiler/Optimization/src/SrtWalker/SrtEvaluator.cpp) evaluates `ConvertS32F32` by rounding toward zero, so a descriptor dword an expression narrows from floating point is computed at run time like the [SPIR-V `OpConvertFToS`](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpConvertFToS) the backend emits. A NaN, an infinity or a value outside the signed 32-bit range is refused instead of converted, as the unsigned sibling refuses its own range, and a plan whose descriptor needs such a value fails to capture. Descriptor expressions that clamp in floating point and then narrow, the shape `convertF32ToI32Saturated` produces, are tested on the host through both the evaluator and its tape; native RDNA2/PS5 execution and title behavior remain unverified.
