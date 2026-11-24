# shader

## change System-dependency builds do not receive

- System-dependency builds do not receive the [parser initialization guard](../../../core/shader/recompiler/PatchSpirvTools.cmake). Their installed SPIRV-Tools must include [upstream #6834](https://github.com/KhronosGroup/SPIRV-Tools/pull/6834), which initializes floating-point encodings before parsing standalone literal-float operands. No title use of `FPMaxErrorDecorationINTEL` is established.

## add System-dependency builds need push-constant preservation

- System-dependency builds need push-constant preservation in their installed optimizer. The bundled [DCE interface patch](../../../core/shader/recompiler/PatchSpirvTools.cmake) keeps global push-constant variables and their dependent declarations when interface preservation is requested. SPIR-V before 1.4 cannot list these globals in `OpEntryPoint`, but prepared binding layouts still require their push-data interface after specialization. Dead function arithmetic remains eligible for elimination.
