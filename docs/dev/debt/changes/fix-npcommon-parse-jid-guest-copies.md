# libSceNpCommon

## add Functional

- [sceNpUtilParseJid guest buffers](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the text is read once through checked copies in page-bounded chunks; the pieces, the `NpJid` and the measured length are built on the host and each written with one checked copy. An unreadable text or an unwritable output returns `NP_UTIL_ERROR_INVALID_ARGUMENT`, the null-text result, with `errno` kept; a piece that doesn't fit no longer leaves the earlier pieces in the buffer. A host-safety fallback, not measured on the console.
