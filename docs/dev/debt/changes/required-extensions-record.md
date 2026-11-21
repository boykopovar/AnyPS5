# relinker

## change 2011 and 2012 Macs

- `--to-avx` ([matcher](../../../core/relinker/codegen/src/x86/Amd64OnlyInstructionMatcher.cpp)) applies the BMI1, BMI2, CLWB and RDPID lowerings of `--to-rosetta` and adds TZCNT, LZCNT and MOVBE, for hosts with AVX but without AVX2, such as Sandy Bridge and Ivy Bridge. AVX2, F16C, FMA, PDEP, PEXT, RDRAND and RDSEED are not lowered, so a title that reaches one still raises an illegal instruction. Linux and Windows output record the extensions the converted code still requires in an `AnyPS5` `PT_NOTE` or a `.aps5cpu` section ([record](../../../core/relinker/codegen/src/x86/RequiredExtensions.cpp)), and the runtime requires only those with the baseline. macOS output has no record because its runtime does not check the host CPU. A conversion that excludes a bundled module records all seven extensions. Undecodable code records all seven too, and the scan follows `.eh_frame` ranges where present, so data decoded as an instruction can only over-require.

# libkernel

## add Required extension record

- [`CheckHostCpu`](../../../core/libs/prx/libkernel/System/src/HostCpu.cpp) reads the `AnyPS5` note or the `.aps5cpu` section of the main image and requires the seven extensions only where the record names them; without a record it keeps requiring all of them. Only `--to-avx` conversions write a record, and a module's requirements are folded into the executable's. The reader accepts version 1 only, rejects unknown extension bits, and falls back to the full check for a malformed record. Native code the relinker does not convert is not scanned.
