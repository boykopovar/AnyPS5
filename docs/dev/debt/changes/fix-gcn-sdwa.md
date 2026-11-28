# shader

## add Unknown function info

- [GCN SDWA encoding](../../../core/shader/recompiler/RdnaDecoder/src/GcnInstructionDecoder.cpp) (shader, PS4) - not measured on a console. The word after a VOP1, VOP2 or VOPC instruction whose first source is 0xF9 is passed to the RDNA decoder unchanged, so it is read in the RDNA layout: SD [15] and SDST [14:8] for compares, S0 [23] and S1 [31]. An SDWA v_cmpx also writes the SGPR pair named by SDST (VCC when SD is clear), which the GCN v_cmpx measurement does not cover. A CUSA00223 PS4 Pro compute shader is consistent with that layout, and [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/7c0b846d0579c7c13b4c2d76032227b68615d068/src/shader_recompiler/frontend/instruction.h#L263-L301) reads the same one. Closes when: measured on a PS4 Pro.
