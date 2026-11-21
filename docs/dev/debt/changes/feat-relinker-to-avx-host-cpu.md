# relinker

## add Functional

- The [`--to-avx` host CPU record](../../../core/relinker/codegen/src/x86/HostCpuFeatureClassifier.cpp) (relinker) is conservative: a 256-bit VEX instruction whose map, prefix and opcode are not in the AVX1 list requires AVX2, a 128-bit one only for the listed AVX2-only forms, so a missed AVX1 opcode refuses a capable host or starts on an incapable one, and data decoded as a recorded opcode over-requires. It records only LZCNT, BMI1, BMI2, MOVBE, AVX2, FMA and F16C; FMA4, XOP, AVX-512, ADX, RDRAND, RDSEED, SHA, CLWB and RDPID are not recorded. The record covers the executable and the modules converted in the same run. No Sandy or Ivy Bridge host was available; the record was checked as bytes, not by starting a title without AVX2.

## change Linux and Windows runtimes stop at start-up

- `--to-avx` ([matcher](../../../core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp)) applies the BMI1, BMI2, CLWB and RDPID lowerings of `--to-rosetta` and adds TZCNT, LZCNT and MOVBE for hosts with AVX but without AVX2. It no longer requires `--macos`. Linux and Windows output records the union over the executable and the same-run bundled modules of the LZCNT, BMI1, BMI2, MOVBE, AVX2, FMA and F16C bits that survive lowering, as a 16-byte `PT_OS_HOSTCPU` header or `.hostcpu` section; libkernel requires the baseline plus those bits and keeps the old all-extension check without a record. AVX2 outside the twin, lane-split, permute and widen lowerings, F16C, FMA, PDEP and PEXT are not lowered, but a retained one is recorded, so start-up refuses it instead of faulting later; RDRAND and RDSEED are neither lowered nor recorded and still fault. The record was checked as bytes, not on a Sandy or Ivy Bridge host.
