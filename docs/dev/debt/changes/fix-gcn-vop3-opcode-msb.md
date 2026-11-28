# shader

## add Unknown function info

- [GCN VOP3 opcodes from 512 up](../../../core/shader/recompiler/RdnaDecoder/src/GcnInstructionDecoder.cpp) (shader, PS4) - not checked on a console. The tenth bit of a VOP3 opcode is read from bit 16, which the Sea Islands ISA reserves, as shadPS4 does for every VOP3 instruction ([decode.cpp](https://github.com/shadps4-emu/shadPS4/blob/6999be18c67e84a7ebe6a654dc4c6616528d0229/src/shader_recompiler/frontend/decode.cpp#L736-L737)); the PS4 Pro opcodes this gives that the VOP3 table maps decode as the RDNA opcodes of the same numbers. A CUSA00223 PS4 Pro compute shader is consistent with it (`v_add3_u32` words that would otherwise be `v_div_scale_f32`s writing over a live image descriptor). One of these opcodes with OP_SEL bits (15:12, shadPS4's layout) set throws. Closes when: measured on a PS4 Pro.
