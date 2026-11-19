# relinker

## change [256-bit integer lowering]

- [256-bit integer lowering](../../../core/relinker/codegen/src/x86/LaneSplitLowering.cpp) under `--to-avx` does not cover variable shifts, VPBLENDVB, VMPSADBW, VPMOVMSKB or masked moves, and each covered instruction runs about ten times slower than on an AVX2 host. It runs the lane-local integer instructions listed in `_shape` as two VEX.128 halves: VEXTRACTF128 into scratch YMM registers saved in a 224-byte frame below the red zone, the 128-bit form per half, then VINSERTF128; RFLAGS is not written. The execution test compares each split with the 128-bit instruction on each half, with aliasing, the other YMM registers and the red zone checked; memory sources are decoded but not executed.

## add Functional

- [Gather lowering](../../../core/relinker/codegen/src/x86/GatherLowering.cpp) under `--to-avx` does not reproduce a fault's partial state: a page fault on an element is reported in the stub with the stack 248 bytes lower, and the destination and mask registers still hold their values from before the gather, where the SDM leaves the elements gathered so far written and their mask elements cleared. The eight VPGATHER and VGATHER forms, 128- and 256-bit, run as one scalar load per element whose mask sign bit is set, in element order, through one saved general register, with RFLAGS kept by PUSHFQ and POPFQ; the mask is then zeroed and the bits above the gathered elements cleared. A gather with 32-bit addressing (`67`) or one that repeats a vector register (#UD) is left unchanged. The execution test compares each form with the SDM model and the host's own gather, with RSP, absent and extended bases, every scale and masked elements at unmapped addresses.
