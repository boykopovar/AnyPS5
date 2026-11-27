# relinker

## change An executable relocation against a symbol the executable defines with `STB_LOCAL`

- An executable relocation against a symbol the executable defines with `STB_LOCAL` binding or `STV_HIDDEN` or `STV_INTERNAL` visibility still fails conversion for 32-bit relocations (`R_X86_64_PC32`, `R_X86_64_32`), for `SHN_ABS` targets outside file-backed writable bytes, and for mixed local and imported `R_X86_64_JUMP_SLOT` tables under `--lazy-binding` without `unused-filter=2` ([pipeline](../../../core/relinker/relinker/src/pipeline/RelinkerPipeline.cpp)); 64-bit relocations (`R_X86_64_64`, `R_X86_64_GLOB_DAT`, `R_X86_64_JUMP_SLOT`) resolve to the definition, which has not been measured on a console.
