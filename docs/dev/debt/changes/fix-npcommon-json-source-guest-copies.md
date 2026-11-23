# libSceNpCommon

## add Functional

- [sceNpUtilJsonEscape, sceNpUtilJsonUnescape and sceNpUtilJsonGetOneChar](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the source text is copied in before it is decoded: `sceNpUtilJsonUnescape` copies all `srcLength` bytes; `sceNpUtilJsonGetOneChar` copies at most the 12 bytes of a surrogate pair; `sceNpUtilJsonEscape` copies up to the first terminator, and no further than the bytes it could append before the buffer is full. A source that can't be read returns `0x80550601` (`NP_UTIL_ERROR_INVALID_ARGUMENT`), as an unwritable buffer does. This is a host-safety fallback: what the console returns for an inaccessible source is not measured. Closes when: compared with the console.
