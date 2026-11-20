# libSceAudioPropagation

## remove | `sceAudioPropagationSourceQueryAudioPaths` | `yW8FldaufkU` |

## change 8 entries present in both version-labelled images

- [Additional callable exports](../../../core/libs/prx/libSceAudioPropagation/InventoryExports.cpp) - 7 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Unknown function info

- [sceAudioPropagationSourceQueryAudioPaths](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - takes `(source, uint64_t* paths, size_t size)` and follows firmware 13.42's export ([reading](https://github.com/boykopovar/AnyPS5/issues/21838#issuecomment-6938983551), disassembly only, not run on a console): an unknown source returns `ErrorInvalidHandle` before a null `paths` returns `ErrorNullPointer`. It then returns the source's path count, the 32-bit `+0x28` that only `SourceSetAudioPaths` and `SourceCalculateAudioPaths` fill, writing one handle per path while they fit in `size / 8` entries. Here `SourceSetAudioPaths` accepts only a count of 0 and `SourceCalculateAudioPaths` throws, so every source has 0 paths and the call returns 0 with nothing written. That count is separate from the 36 path slots `sceAudioPropagationSourceGetAudioPathCount` reports here, which the reading doesn't cover.
