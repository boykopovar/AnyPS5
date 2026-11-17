# relinker

## change applies the BMI1, BMI2, CLWB and RDPID lowerings

- `--to-avx` ([matcher](../../../core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp)) applies the BMI1, BMI2, CLWB and RDPID lowerings of `--to-rosetta` and adds TZCNT, LZCNT and MOVBE, for hosts with AVX but without AVX2, such as the Sandy Bridge and Ivy Bridge CPUs of 2011 and 2012 Macs. AVX2, F16C, double-precision FMA (the PD and SD forms), PDEP, PEXT, RDRAND and RDSEED are not lowered, so a title that reaches one of them still raises an illegal instruction; Sandy Bridge has neither RDRAND nor RDSEED, so the `--to-rosetta` RDSEED-to-RDRAND rewrite would not help there. It requires `--macos`: the Linux and Windows runtimes stop at start-up when the host lacks BMI1, BMI2, LZCNT, MOVBE or AVX2 and do not yet read which of them a conversion lowered.

## add Functional

- [Single-precision FMA](../../../core/relinker/codegen/src/x86/FmaLowering.cpp) under `--to-avx` runs the PS and SS forms of VFMADD, VFMSUB, VFNMADD, VFNMSUB, VFMADDSUB and VFMSUBADD without FMA: the product is formed exactly in double precision, the sum is rounded to odd with its exact TwoSum error, and narrowing it in the guest's rounding mode gives the singly rounded result. NaN selection, the sign of an exact zero, DAZ, FTZ and the status flags follow the hardware, but an unmasked exception does not trap: the result is written and only the flag is set. Each instruction becomes about 175 instructions (128-bit and scalar) or 300 (256-bit), with thirteen scratch YMM registers saved below the red zone; RFLAGS and the general-purpose registers are not written. Precise fault delivery inside the stub has not been verified.
