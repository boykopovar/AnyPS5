# shader

## change System-dependency builds do not receive

- System-dependency builds do not receive the [parser initialization guard](../../../core/shader/recompiler/PatchSpirvTools.cmake). Their installed SPIRV-Tools must include [upstream #6834](https://github.com/KhronosGroup/SPIRV-Tools/pull/6834), which initializes floating-point encodings before parsing standalone literal-float operands. No title use of `FPMaxErrorDecorationINTEL` is established.
