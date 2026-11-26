# relinker

## change [MULX lowering]

- Execution of a title using [MULX lowering](../../../core/relinker/codegen/src/x86/MulxLowering.cpp) under Rosetta and precise fault delivery inside its spill sequence have not been verified. The lowering test passes under Rosetta on an M5 with macOS 26.2. The Rosetta host observations below predate this lowering for MULX. The stub uses 192 bytes below RSP, leaving the 128-byte red zone intact; memory operands are loaded before allocating the spill frame so stack aliases and signed disp32 displacements retain their original effective address. Under the Rosetta target, a memory-form MULX whose two destinations are both in {RDX, RSP} is kept native and reported because no safe destination register is available to preload the source; runtime support for the retained instruction is not verified. The AVX-only target rejects this form rather than emitting an instruction that requires BMI2.
