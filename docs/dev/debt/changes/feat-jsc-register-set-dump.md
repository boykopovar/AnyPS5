# libSceJsc

## add Functional

- [JSC::RegisterSet::dump](../../../core/libs/prx/libSceJsc/src/RegisterSetDump.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. It follows WebKit 73a35b43 [RegisterSet.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/jit/RegisterSet.cpp#L345-L354): `[`, then each set register of the 8-byte bitmap (bits 0-15 `%rax`..`%r15`, 16-31 `%xmm0`..`%xmm15`, from X86_64Registers.h) after a `CommaPrinter`, then `]`, with the inlined `print` begin/vprintf/end nesting. The bit layout matches the firmware 13.42 `allGPRs`/`allFPRs`/`calleeSaveRegisters` values; bit 32 and above are not printed. Closes when: compared with a console module.

## remove | `_ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE` | `WW0x+WVuT58` |
