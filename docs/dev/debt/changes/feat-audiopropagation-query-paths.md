# libSceAudioPropagation

## remove | `sceAudioPropagationSourceQueryAudioPaths` | `yW8FldaufkU` |

## add Unknown function info

- [sceAudioPropagationSourceQueryAudioPaths](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - takes `(source, uint64_t* paths, size_t size)` and follows firmware 13.42's export ([reading](https://github.com/boykopovar/AnyPS5/issues/21838#issuecomment-6938983551), disassembly only, not run on a console): an unknown source returns `ErrorInvalidHandle` before a null `paths` returns `ErrorNullPointer`; then it writes one 64-bit path handle per slot while they fit in `size / 8` entries and returns the full path count, so a short buffer still learns it. The count is the source's 36 slots, as `sceAudioPropagationSourceGetAudioPathCount` reports here; 13.42 keeps a count field that is 0 until paths exist, and what fills it wasn't read. An unwritable `paths` returning `ErrorNullPointer` is a host-safety fallback.
