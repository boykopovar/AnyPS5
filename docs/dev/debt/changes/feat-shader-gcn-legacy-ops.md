# shader

## add Functional

- [GCN v_mov_fed_b32 opcode](../../../core/shader/recompiler/RdnaDecoder/src/GcnInstructionDecoder.cpp) - Hardware-exact FED (first execute default) exception semantics are unmodeled on console hardware. GCN VOP1 and VOP3 v_mov_fed_b32 opcodes decode to canonical RDNA v_mov_b32 instructions. Closes when: measured on hardware.
  Labels: console, gpu, tested.
