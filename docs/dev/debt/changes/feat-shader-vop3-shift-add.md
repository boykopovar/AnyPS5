# shader

## add Functional

- [VOP3 source modifiers on v_lshl_add_u32 and v_add_lshl_u32](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - Console silicon (gfx1013) behavior for VOP3 shift-add source modifiers remains unmeasured. Floating-point sign-bit abs and neg operand modifiers are supported based on native measurements on gfx1036 and gfx1201 hardware, while clamp, omod, and op_sel continue to throw translation exceptions until verified on console hardware. Closes when: measured on console hardware.
  Labels: console, gpu, tested.
