# shader

## add Functional

- [Integer DPP16 and SDWA source modifiers](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `neg` and `abs` on a 32-bit integer VOP1 or VOP2 source change bit 31 of the operand after the DPP lane move and after the SDWA select and extension: `neg` flips it, `abs` clears it, both set it. Measured for every opcode the decoder accepts them on (`isVop1IntegerSourceOpcode` and the integer SDWA profile) on RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010) through the [hardware oracle](../HW_ORACLE.md) in wave32, a subset also in wave64.
