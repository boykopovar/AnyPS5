# relinker

## change [256-bit integer lowering]

- [256-bit integer lowering](../../../core/relinker/codegen/src/x86/LaneSplitLowering.cpp) under `--to-avx` does not cover VPBLENDVB, VMPSADBW, VPMOVMSKB, gathers or masked moves, and each covered instruction runs about ten times slower than on an AVX2 host. It runs the lane-local integer instructions listed in `_shape` as two VEX.128 halves: VEXTRACTF128 into scratch YMM registers saved in a 224-byte frame below the red zone, the 128-bit form per half, then VINSERTF128; RFLAGS is not written. The execution test compares each split with the 128-bit instruction on each half, with aliasing, the other YMM registers and the red zone checked; memory sources are decoded but not executed.

## add Functional

- [Variable shift lowering](../../../core/relinker/codegen/src/x86/VariableShiftLowering.cpp) has not run on a host with AVX and without AVX2, and no title has been converted with it. It replaces VPSLLVD, VPSLLVQ, VPSRLVD, VPSRLVQ and VPSRAVD, in their VEX.128 and VEX.256 forms, with per-element scalar shifts by CL in a 256-byte frame below the red zone, selecting the saturated result with CMOVAE because the scalar shifts mask their count where the AVX2 instructions saturate. The hardware oracle compares the stub with the instruction itself only where the host agrees with the Intel SDM: Rosetta 2 (measured on an M5 Pro, macOS 26.7.1) decides VPSLLVQ and VPSRLVQ saturation from the low 32 bits of each 64-bit count, so a count of 0x0000000100000001 shifts by 1 there instead of producing zero. The oracle probes one such count and skips that pattern only on a host that disagrees. Filed with Apple as FB25600143. Closes when: executed on a host with AVX and without AVX2.
