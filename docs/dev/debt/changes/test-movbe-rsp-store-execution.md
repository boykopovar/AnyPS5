# relinker

## change [MOVBE lowering]

- [MOVBE lowering](../../../core/relinker/codegen/src/x86/MovbeLowering.cpp) moves through a scratch register with MOV and BSWAP; a 16-bit form swaps 32 bits and takes the upper half through a stack slot with MOVZX, so no instruction in the stub writes RFLAGS, which MOVBE leaves unchanged. A 16-bit load keeps bits 63:16 of the destination. The execution test covers RSP-relative loads and stores and verifies stored bytes and preserved surrounding stack memory. Precise fault delivery inside a spill sequence has not been verified.
