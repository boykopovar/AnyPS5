# shader

## change System-dependency builds do not receive

- System-dependency builds require SPIRV-Tools [upstream #6834](https://github.com/KhronosGroup/SPIRV-Tools/pull/6834) for initialized floating-point encodings of standalone literal-float operands. No title use of `FPMaxErrorDecorationINTEL` is established.
