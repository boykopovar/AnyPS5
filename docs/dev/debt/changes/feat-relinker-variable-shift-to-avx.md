# relinker

## change [256-bit integer lowering]

- [256-bit integer lowering](../../../core/relinker/codegen/src/x86/LaneSplitLowering.cpp) under `--to-avx` does not cover VPBLENDVB, VMPSADBW, VPMOVMSKB, gathers or masked moves, and each covered instruction runs about ten times slower than on an AVX2 host. It runs the lane-local integer instructions listed in `_shape` as two VEX.128 halves: VEXTRACTF128 into scratch YMM registers saved in a 224-byte frame below the red zone, the 128-bit form per half, then VINSERTF128; RFLAGS is not written. The execution test compares each split with the 128-bit instruction on each half, with aliasing, the other YMM registers and the red zone checked; memory sources are decoded but not executed.

## add Functional

- [Variable shift lowering](../../../core/relinker/codegen/src/x86/VariableShiftLowering.cpp) under `--to-avx` has not run on a CPU without AVX2, and precise fault delivery inside its stub has not been verified. It runs VPSLLVD, VPSLLVQ, VPSRLVD, VPSRLVQ and VPSRAVD, VEX.128 and VEX.256, as one uniform shift per element (VPSLLD, VPSLLQ, VPSRLD, VPSRLQ or VPSRAD by a count isolated with VPSHUFD and VPMOVZXDQ, or VPUNPCKHQDQ for quadwords) merged with VBLENDPS or VBLENDPD; a uniform shift reads a 64-bit count, so counts past the element width give zero or the sign as the variable forms do. A 256-bit shift becomes about 45 instructions, with up to seven scratch YMM registers saved below the red zone; RFLAGS is not written. The execution test checks every YMM register, the memory operand and the red zone against the SDM model; the stubs matched one host's AVX2 instructions on 160,000 random cases outside this repository.
