# relinker

## change [256-bit integer lowering]

- [256-bit integer lowering](../../../core/relinker/codegen/src/x86/LaneSplitLowering.cpp) under `--to-avx` does not cover variable shifts, VPBLENDVB, VMPSADBW, gathers or masked moves, and each covered instruction runs about ten times slower than on an AVX2 host. It runs the lane-local integer instructions listed in `_shape` as two VEX.128 halves: VEXTRACTF128 into scratch YMM registers saved in a 224-byte frame below the red zone, the 128-bit form per half, then VINSERTF128; RFLAGS is not written. The execution test compares each split with the 128-bit instruction on each half, with aliasing, the other YMM registers and the red zone checked; memory sources are decoded but not executed.

## add Functional

- [VPMOVMSKB lowering](../../../core/relinker/codegen/src/x86/MoveMaskLowering.cpp) under `--to-avx` runs the 256-bit form as the VEX.128 form on the low half and on the upper half taken with VEXTRACTF128, storing each 16-bit mask with a 16-bit MOV into a slot below the red zone and loading the 32-bit result with MOV, which zero-extends it as the SDM entry for VPMOVMSKB does; one scratch YMM register is saved in a 176-byte frame and RFLAGS is not written. An RSP destination fails the conversion. The execution test checks every destination and source register, the other GPRs and YMM registers, RFLAGS and the red zone against the SDM result, and against the host instruction where it has AVX2.
