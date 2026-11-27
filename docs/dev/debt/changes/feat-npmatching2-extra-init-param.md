# libSceNpMatching2

## remove [sceNpMatching2SetExtraInitParam]

## add Silent stubs

- [sceNpMatching2SetExtraInitParam](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - takes `(const void* param)` and follows firmware 13.42's export ([reading](https://github.com/boykopovar/AnyPS5/issues/21836#issuecomment-6938862410), disassembly only, not run on a console): a null `param` returns `INVALID_ARGUMENT`, with no initialization check before or after `sceNpMatching2Initialize`. It reads bit 0 of byte `+0` and the 64-bit `+8`, stores the pair {8000000, `+8` or 20000000 when 0} and, when the bit is set, the 16-bit value `0x245b`, and returns 0. What the module uses those stored values for wasn't read, so nothing here reads them back. An unreadable `param` returning `INVALID_ARGUMENT` is a host-safety fallback. Closes when: the readers of the stored values are traced.
