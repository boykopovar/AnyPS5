# libSceNpCommon

## add Functional

- [sceNpRtcFormatRFC3339 terminator](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - the terminator at the end of the caller's buffer is written through a checked copy; a failed write returns `0x80B50002`, the invalid-pointer result `sceRtcFormatRFC3339` gives, with `errno` kept. A host-safety fallback, not measured on the console.
